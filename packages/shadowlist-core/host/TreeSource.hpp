#pragma once

#include <shadowlist-core/host/TreeRows.hpp>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace azimgd::shadowlist {

/*
 * A host's tree encoded depth first, including collapsed descendants. Keys are unique and
 * depths describe its topology. The host keeps its node objects; this source owns only their
 * visible rows and expansion intent, including the branches hidden by a collapsed parent.
 */
class TreeSource final {
public:
  /*
   * Replace topology, keeping expansion intent for keys whose children arrive later. Explicit
   * expanded flags seed the intent; setExpandedKeys replaces it, including unknown or leaf keys.
   */
  void reset(std::vector<TreeRow> rows);
  void setExpandedKeys(const std::vector<std::string>& keys);
  std::vector<std::string> expandedKeys() const;
  void toggle(const std::string& key);
  TreeSplice expand(std::size_t row);
  TreeSplice collapse(std::size_t row);
  const TreeRows& rows() const { return visible_; }

private:
  std::vector<TreeRow> visibleFrom(std::size_t start, std::uint32_t depth) const;
  void rebuild();

  std::vector<TreeRow> source_;
  std::unordered_map<std::string, std::size_t> indices_;
  std::unordered_set<std::string> expanded_;
  TreeRows visible_;
};

}
