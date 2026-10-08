#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace azimgd::shadowlist {

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
