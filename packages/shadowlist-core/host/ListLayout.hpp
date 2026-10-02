#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <shadowlist-core/Container.hpp>

namespace azimgd::shadowlist {

/*
 * A mounted row the host laid out: its row index, its measured size and an id the host
 * knows it by, like a view tag.
 */
struct MeasuredRow {
  std::size_t elementIndex = 0;
  double width = 0.0;
  double height = 0.0;
  std::uint64_t id = 0;
};

/*
 * Give the core the measured header, footer and window size and reflow the rows. The first
 * layout is right even though the core's frame ran before the host laid the list out. Returns
 * whether anything changed, which marks the offset corrected so the host writes it again.
 */
bool applyLayoutInputs(Container& core, double headerSize, double footerSize, double windowWidth, double windowHeight);

/*
 * Give the core every mounted row's size, then reflow once from the lowest changed row.
 * In a multi column list the column sets the cross size. Only the scroll axis size is
 * taken. Ids of rows measured for the first time go into firstMeasured.
 */
void applyMeasuredRows(Container& core, const std::vector<MeasuredRow>& rows, bool horizontal,
  std::vector<std::uint64_t>& firstMeasured);

/*
 * Where the core places a row. Offsets already include the header. A multi column list also
 * sets the width.
 */
struct RowFrame {
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  bool setsWidth = false;
};

RowFrame rowFrame(const Container& core, std::size_t elementIndex, bool horizontal);

/*
 * Where each template sits along the scroll axis. The header and the section header overlay
 * sit at the start, the empty template right after the header, the footer after the content.
 */
struct TemplateOffsets {
  double header = 0.0;
  double empty = 0.0;
  double footer = 0.0;
  double sectionHeader = 0.0;
};

TemplateOffsets templateOffsets(const Container& core, double headerSize, double footerSize);

/*
 * Sticky header and snap positions a host needs to pin headers and snap on its own thread.
 * They are rebuilt only when the core's geometry moved, and a list's pointer only changes with
 * its values. Null means empty.
 */
class PublishedGeometry final {
public:
  /*
   * Rebuild from the core if its geometry moved. Returns whether anything was rebuilt.
   */
  bool refresh(const Container& core);

  std::shared_ptr<const std::vector<int>> stickyHeaderIndices;
  std::shared_ptr<const std::vector<double>> stickyHeaderOffsets;
  std::shared_ptr<const std::vector<double>> stickyHeaderSizes;
  std::shared_ptr<const std::vector<double>> snapOffsets;

private:
  // 0 means nothing cached yet.
  std::uint64_t geometryVersion_ = 0;
  bool snapToItem_ = false;
  int snapAlignment_ = -1;
  bool inverted_ = false;
  bool horizontal_ = false;
  double windowSize_ = -1.0;
  double totalSize_ = -1.0;
  std::vector<std::size_t> sourceStickyIndices_;
};

/*
 * Show a hidden row anyway after this many layout passes, in case corrections never settle.
 */
constexpr std::size_t MAX_CONCEALED_LAYOUT_PASSES = 8;

/*
 * Rows hidden until their offset correction is on screen, by host id. A row first measured
 * above the anchor stays hidden until a host report echoes its generation with no correction
 * pending. Payload is whatever the host needs to hide and show the row, like its props.
 */
template <typename Payload>
class ConcealTracker final {
public:
  struct Row {
    Payload payload;
    std::uint64_t generation = 0;
    std::size_t layoutPasses = 0;
  };

  /*
   * The row index to hide first measured rows before, or 0 for none.
   */
  static std::size_t hideBeforeIndex(const Container& core, bool correcting, bool anyFirstMeasured) {
    if (!correcting || !anyFirstMeasured) {
      return 0;
    }
    const Anchor* anchor = core.compensationAnchor();
    if (anchor == nullptr || anchor->key.empty()) {
      return 0;
    }
    std::size_t anchorIndex = core.findElementIndexByKey(anchor->key);
    return anchorIndex == UNDEFINED_INDEX ? 0 : anchorIndex;
  }

  Row* find(std::uint64_t id) {
    auto entry = rows_.find(id);
    return entry == rows_.end() ? nullptr : &entry->second;
  }

  /*
   * Count one more layout pass for a hidden row. Returns whether it may show again: the host
   * echoed its generation with no correction pending, or it waited too long.
   */
  static bool settle(Row& row, std::uint64_t ack, bool correcting) {
    ++row.layoutPasses;
    return (ack >= row.generation && !correcting) || row.layoutPasses > MAX_CONCEALED_LAYOUT_PASSES;
  }

  void show(std::uint64_t id) {
    rows_.erase(id);
  }

  /*
   * Forget hidden rows that are no longer mounted. stillHidden need not be sorted.
   */
  void forgetExcept(std::vector<std::uint64_t>& stillHidden) {
    if (rows_.size() <= stillHidden.size()) {
      return;
    }
    std::sort(stillHidden.begin(), stillHidden.end());
    for (auto entry = rows_.begin(); entry != rows_.end();) {
      bool keep = std::binary_search(stillHidden.begin(), stillHidden.end(), entry->first);
      entry = keep ? std::next(entry) : rows_.erase(entry);
    }
  }

  bool empty() const {
    return rows_.empty();
  }

  /*
   * The newest hide while any row is hidden, or 0 when nothing waits on the host.
   */
  double publishedGeneration() const {
    return rows_.empty() ? 0.0 : static_cast<double>(generation_);
  }

  /*
   * Start a layout pass. Every row hidden in one pass gets the same generation.
   */
  void beginPass() {
    passGeneration_ = generation_ + 1;
  }

  std::uint64_t hide(std::uint64_t id, Payload payload) {
    generation_ = passGeneration_;
    rows_.insert_or_assign(id, Row{std::move(payload), passGeneration_, 0});
    return passGeneration_;
  }

private:
  std::unordered_map<std::uint64_t, Row> rows_;
  // The newest generation given to a hide, or 0 if nothing was ever hidden.
  std::uint64_t generation_ = 0;
  std::uint64_t passGeneration_ = 1;
};

}
