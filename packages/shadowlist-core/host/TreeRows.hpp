#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * One visible row of a tree: its node's key, how deep the node is (0 for a root), whether it has
 * children and whether they show.
 */
struct TreeRow {
  std::string key;
  std::uint32_t depth = 0;
  bool hasChildren = false;
  bool expanded = false;
};

/*
 * How an expand or collapse changed the rows: deleted rows from start went away and inserted
 * rows took their place.
 */
struct TreeSplice {
  std::size_t start = 0;
  std::size_t deleted = 0;
  std::size_t inserted = 0;
};

/*
 * The rows of a tree whose nodes' parents are all expanded, depth first, collapsed branches
 * left out. A node's visible descendants are the run of rows after it that are deeper than it,
 * which expand and collapse splice in and out without walking the rest of the tree. The host
 * walks its own data for the rows an expand shows, since only it knows a node's children.
 */
class TreeRows final {
public:
  void reset(std::vector<TreeRow> rows);

  std::size_t size() const { return keys_.size(); }
  const std::vector<std::string>& keys() const { return keys_; }
  TreeRow rowAt(std::size_t row) const;
  std::uint32_t depthAt(std::size_t row) const { return depths_[row]; }

  /*
   * The first row after row's visible descendants, or size(). row + 1 for a collapsed node.
   */
  std::size_t subtreeEnd(std::size_t row) const;

  /*
   * Shows descendants below row, which must be depth first rows deeper than it. Rows already
   * showing under row are replaced. Nothing changes for a row past the end or without children.
   */
  TreeSplice expand(std::size_t row, std::vector<TreeRow> descendants);

  /*
   * Hides row's visible descendants and marks it collapsed.
   */
  TreeSplice collapse(std::size_t row);

private:
  std::vector<std::string> keys_;
  std::vector<std::uint32_t> depths_;

  /*
   * HAS_CHILDREN and EXPANDED per row.
   */
  std::vector<std::uint8_t> flags_;
};

constexpr std::uint8_t TREE_ROW_HAS_CHILDREN = 1;
constexpr std::uint8_t TREE_ROW_EXPANDED = 2;

}
