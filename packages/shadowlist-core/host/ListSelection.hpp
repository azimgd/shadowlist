#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace azimgd::shadowlist {

/*
 * What a tap on a row did. selected is a tap that selected the row, or kept it selected under
 * single selection. toggledOff is a tap that deselected the row under multiple selection.
 * deselected has the other rows the tap deselected.
 */
struct SelectionTap {
  bool selected = false;
  bool toggledOff = false;
  std::vector<std::string> deselected;
};

/*
 * The selected rows of a list, held by key. A selection follows its rows across inserts and
 * moves, and a removed row leaves it. With single selection, selecting a row deselects the one
 * selected before.
 */
class ListSelection final {
public:
  void setMultiple(bool multiple);
  bool isMultiple() const { return multiple_; }

  /*
   * Select a row. Rows this deselects, the previous one under single selection, are added to
   * deselected. Returns whether the row was not selected before.
   */
  bool select(const std::string& key, std::vector<std::string>& deselected);

  /*
   * Returns whether the row was selected.
   */
  bool deselect(const std::string& key);

  /*
   * A tap on a row. Multiple selection toggles it. Otherwise the row is selected and the one
   * selected before is deselected. shouldSelect is asked before a row gets selected, never
   * before one is toggled off. A false answer changes nothing.
   */
  template <typename ShouldSelect>
  SelectionTap tap(const std::string& key, ShouldSelect shouldSelect) {
    SelectionTap result;
    if (multiple_ && contains(key)) {
      deselect(key);
      result.toggledOff = true;
      return result;
    }
    if (!shouldSelect()) {
      return result;
    }
    select(key, result.deselected);
    result.selected = true;
    return result;
  }

  /*
   * Deselect every row and add them to deselected.
   */
  void clear(std::vector<std::string>& deselected);

  bool contains(const std::string& key) const { return keys_.count(key) > 0; }
  bool isEmpty() const { return keys_.empty(); }
  std::size_t getCount() const { return keys_.size(); }

  /*
   * Positions of the selected keys in keys, low to high.
   */
  std::vector<std::size_t> indicesIn(const std::vector<std::string>& keys) const;

  /*
   * Drop the selected keys that keys no longer has.
   */
  void retainKeys(const std::vector<std::string>& keys);

private:
  bool multiple_ = false;
  std::unordered_set<std::string> keys_;
};

}
