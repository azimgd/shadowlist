#pragma once

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/RowSizeSpec.hpp>
#include <shadowlist-core/host/SharedKeys.hpp>
#include <shadowlist-core/host/TreeRows.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace azimgd::shadowlist {

/*
 * One version of a list's row keys, never changed once published. A plain list holds its keys,
 * a sectioned list its sections and the header rows that stick, a tree its visible rows. Keys a
 * version keeps from its base are shared with it, see SharedKeys. keys() stays valid while the
 * snapshot lives.
 */
class RowKeysSnapshot final {
public:
  enum class Kind { Plain, Sections, Tree };

  RowKeysSnapshot() = default;
  RowKeysSnapshot(const RowKeysSnapshot&) = delete;
  RowKeysSnapshot& operator=(const RowKeysSnapshot&) = delete;

  std::uint64_t getVersion() const { return version_; }
  Kind getKind() const { return kind_; }
  KeyList keys() const { return kind_ == Kind::Tree ? tree_.keys() : keys_.list(); }
  std::size_t size() const { return keys().size(); }

  /*
   * The version this one was spliced from and how, as an edit the core can trust when it last
   * took exactly that version. Unknown otherwise.
   */
  std::uint64_t getBaseVersion() const { return baseVersion_; }
  const KeyEdit& getEdit() const { return edit_; }

  /*
   * Sorted rows that stick once scrolled past: a sectioned list's header rows when its headers
   * stick. Empty otherwise.
   */
  const std::vector<std::size_t>& getStickyRows() const { return stickyRows_; }

  const ListSections& getSections() const { return sections_; }
  const std::vector<std::string>& getSectionKeys() const { return sectionKeys_; }
  const TreeRows& getTree() const { return tree_; }

  /*
   * A key's row, the first one when it repeats, or UNDEFINED_INDEX. With a hint the rows around
   * it are tried first, HINT_WINDOW each way, and the full index is only built when they miss.
   * Built once per snapshot, on the first lookup that needs it. Safe from any thread.
   */
  static constexpr std::size_t HINT_WINDOW = 64;
  std::size_t indexOfKey(const std::string& key, std::size_t hint = UNDEFINED_INDEX) const;

private:
  friend class RowKeyStore;

  std::uint64_t version_ = 0;
  std::uint64_t baseVersion_ = 0;
  Kind kind_ = Kind::Plain;
  KeyEdit edit_;
  SharedKeys keys_;
  std::vector<std::size_t> stickyRows_;
  ListSections sections_;
  std::vector<std::string> sectionKeys_;
  bool qualifiedItems_ = false;
  TreeRows tree_;

  mutable std::once_flag indexOnce_;
  mutable std::unordered_map<std::string_view, std::size_t> index_;
};

/*
 * What a sectioned list sends for a new version. reuse has one entry per section: the base's
 * section whose rows this one keeps as they are, or UNDEFINED_INDEX for a section whose item
 * keys follow in itemKeys, every such section's items in order. sectionKeys has one key per
 * section.
 */
struct SectionsUpdate {
  bool hasHeader = false;
  bool hasFooter = false;
  bool stickyHeaders = false;
  bool qualifyItems = true;
  std::vector<std::string> sectionKeys;
  std::vector<std::size_t> itemCounts;
  std::vector<std::size_t> reuse;
  std::vector<std::string> itemKeys;
};

/*
 * A list's row keys by version, written by its renderer and read by each commit. The keys never
 * go through props. The renderer writes a new version for new data, often as a change to
 * the one before, and the commit reads the version its props name. A commit's version marks the
 * versions before the previous commit's as unreachable, and they are dropped. Thread safe.
 * Versions only grow. A write naming a base that is gone fails, and the renderer then writes
 * the whole list.
 */
class RowKeyStore final {
public:
  /*
   * Most versions kept past the one last committed, for renders that never commit.
   */
  static constexpr std::size_t MAX_PENDING_VERSIONS = 8;

  bool setKeys(std::uint64_t version, std::vector<std::string> keys);

  /*
   * base's keys with deleteCount keys from start replaced by inserted.
   */
  bool spliceKeys(
    std::uint64_t version,
    std::uint64_t baseVersion,
    std::size_t start,
    std::size_t deleteCount,
    std::vector<std::string> inserted);

  /*
   * Sections built from update, reusing rows of base, which is 0 for none.
   */
  bool setSections(std::uint64_t version, std::uint64_t baseVersion, SectionsUpdate update);

  bool resetTree(std::uint64_t version, std::vector<TreeRow> rows);
  std::optional<TreeSplice> expandTree(
    std::uint64_t version,
    std::uint64_t baseVersion,
    std::size_t row,
    std::vector<TreeRow> descendants);
  std::optional<TreeSplice> collapseTree(std::uint64_t version, std::uint64_t baseVersion, std::size_t row);

  std::shared_ptr<const RowKeysSnapshot> snapshot(std::uint64_t version) const;

  /*
   * A commit ran on version. Drops what no commit can name any more.
   */
  void adopted(std::uint64_t version);

  /*
   * Row size specs by version, kept like the keys.
   */
  void setSizeSpecs(std::uint64_t version, std::vector<RowSizeSpec> specs);
  std::shared_ptr<const std::vector<RowSizeSpec>> sizeSpecs(std::uint64_t version) const;
  void adoptedSizeSpecs(std::uint64_t version);

  std::size_t versionCount() const;

private:
  template <typename T>
  struct Versions {
    std::map<std::uint64_t, std::shared_ptr<const T>> entries;
    std::uint64_t adopted = 0;
    std::uint64_t previousAdopted = 0;

    void put(std::uint64_t version, std::shared_ptr<const T> value);
    std::shared_ptr<const T> find(std::uint64_t version) const;
    void adopt(std::uint64_t version);
  };

  bool publish(std::shared_ptr<RowKeysSnapshot> snapshot);
  std::shared_ptr<const RowKeysSnapshot> baseOf(std::uint64_t baseVersion, RowKeysSnapshot::Kind kind) const;

  mutable std::mutex mutex_;
  Versions<RowKeysSnapshot> keys_;
  Versions<std::vector<RowSizeSpec>> specs_;
};

/*
 * A splice of a list of size keys as an edit the core can trust: an end edit when it only adds
 * or only removes keys at an end, a Splice otherwise. Unknown when it changes nothing, replaces
 * every key or falls outside the list.
 */
KeyEdit keyEditOf(std::size_t size, std::size_t start, std::size_t deleted, std::size_t inserted);

}
