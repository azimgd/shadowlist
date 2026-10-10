/*
 * Key edits at either end of the list take fast paths that keep the rows and the key map in
 * place: appends, prepends and trims at the start or the end. These tests check every path
 * leaves the same rows, sizes and key lookups the full rebuild would, and that duplicate
 * keys still go through the rebuild.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <string>
#include <unordered_map>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * A container with keys whose rows are all measured with a height that follows the key.
 */
void measureAll(Container& container) {
  for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
    const std::string& key = container.revision.elements[index].key;
    double height = 60.0 + static_cast<double>(std::hash<std::string>{}(key) % 100);
    Virtualizer::applyElementSize(container, index, {WINDOW_WIDTH, height});
  }
  Virtualizer::commitElementSizes(container, 0);
}

/*
 * Every key finds its own row, rows are numbered and stacked, and measured sizes still
 * follow their keys.
 */
void checkRows(const Container& container, const std::vector<std::string>& keys) {
  CHECK_EQ(container.revision.elements.size(), keys.size());
  double expected = container.headerSize;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const Element& element = container.revision.elements[index];
    CHECK_EQ(element.key, keys[index]);
    CHECK_EQ(container.findElementIndexByKey(keys[index]), index);
    CHECK_EQ(element.index, index);
    CHECK_NEAR(element.offsetY, expected, 0.0001);
    if (element.measured) {
      double height = 60.0 + static_cast<double>(std::hash<std::string>{}(element.key) % 100);
      CHECK_NEAR(element.height, height, 0.0001);
    }
    expected += element.height;
  }
}

std::vector<std::string> slice(const std::vector<std::string>& keys, std::size_t first, std::size_t last) {
  return std::vector<std::string>(keys.begin() + first, keys.begin() + last);
}

}

TEST(reconcile_trim_end_keeps_rows_sizes_and_lookups) {
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0));
  measureAll(container);

  std::vector<std::string> trimmed = slice(keys, 0, 150);
  CHECK_EQ(Virtualizer::reconcileElements(container, trimmed), static_cast<std::size_t>(150));
  Virtualizer::update(container, inputFor(trimmed, 0.0));
  checkRows(container, trimmed);
  CHECK_EQ(container.findElementIndexByKey("k170"), UNDEFINED_INDEX);
}

TEST(reconcile_trim_start_keeps_rows_sizes_and_lookups) {
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0));
  measureAll(container);

  std::vector<std::string> trimmed = slice(keys, 30, 200);
  CHECK_EQ(Virtualizer::reconcileElements(container, trimmed), static_cast<std::size_t>(170));
  Virtualizer::update(container, inputFor(trimmed, 0.0));
  checkRows(container, trimmed);
  CHECK_EQ(container.findElementIndexByKey("k10"), UNDEFINED_INDEX);
}

TEST(reconcile_edits_at_both_ends_in_turn_keep_lookups_right) {
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0));
  measureAll(container);

  // Prepend, trim the start, append, trim the end, then a regroup through the rebuild.
  std::vector<std::string> next = keysFor(20, "p");
  next.insert(next.end(), keys.begin(), keys.end());
  Virtualizer::update(container, inputFor(next, 0.0));
  checkRows(container, next);

  next = slice(next, 35, next.size());
  Virtualizer::update(container, inputFor(next, 0.0));
  checkRows(container, next);

  std::vector<std::string> appended = keysFor(15, "a");
  next.insert(next.end(), appended.begin(), appended.end());
  Virtualizer::update(container, inputFor(next, 0.0));
  checkRows(container, next);

  next = slice(next, 0, next.size() - 40);
  Virtualizer::update(container, inputFor(next, 0.0));
  checkRows(container, next);

  std::swap(next[3], next[40]);
  next.erase(next.begin() + 10);
  Virtualizer::update(container, inputFor(next, 0.0));
  checkRows(container, next);
}

TEST(reconcile_trim_with_duplicate_keys_hands_the_key_to_the_survivor) {
  std::vector<std::string> keys = keysFor(50);
  keys[40] = keys[2];
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0));
  CHECK(container.revision.hasDuplicateKeys);

  // Row 2 goes with the trim. Its key's other copy at 40 now owns the lookup.
  std::vector<std::string> trimmed = slice(keys, 5, 50);
  Virtualizer::update(container, inputFor(trimmed, 0.0));
  CHECK_EQ(container.findElementIndexByKey(keys[2]), static_cast<std::size_t>(35));
  CHECK(!container.revision.hasDuplicateKeys);
}

TEST(reconcile_appended_duplicate_marks_the_map) {
  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0));
  CHECK(!container.revision.hasDuplicateKeys);
  keys.push_back("k3");
  Virtualizer::update(container, inputFor(keys, 0.0));
  CHECK(container.revision.hasDuplicateKeys);
  CHECK_EQ(container.findElementIndexByKey("k3"), static_cast<std::size_t>(3));
}

TEST(reconcile_small_prepends_do_not_rehash_the_key_map_each_time) {
  std::vector<std::string> keys = keysFor(5000);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0));
  std::size_t rehashes = 0;
  for (int round = 0; round < 100; ++round) {
    std::size_t buckets = container.revision.elementIndexByKey.bucket_count();
    std::vector<std::string> next = keysFor(10, "p" + std::to_string(round) + "-");
    next.insert(next.end(), keys.begin(), keys.end());
    keys = std::move(next);
    Virtualizer::update(container, inputFor(keys, 0.0));
    rehashes += container.revision.elementIndexByKey.bucket_count() != buckets ? 1 : 0;
  }
  CHECK(rehashes <= 3);
  checkRows(container, keys);
}

/*
 * An edit inside the list keeps the rows before and after it with their sizes, and every
 * lookup right, whichever side gets renumbered and with a bias from earlier prepends.
 * Survivors are counted, and a key repeated outside the edit falls back to the full match.
 */
TEST(reconcile_edits_in_the_middle_keep_rows_sizes_and_lookups) {
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0));
  measureAll(container);

  auto apply = [&](const std::vector<std::string>& next, std::size_t survivors) {
    CHECK_EQ(Virtualizer::reconcileElements(container, next), survivors);
    Virtualizer::update(container, inputFor(next, 0.0));
    checkRows(container, next);
    keys = next;
  };

  // A prepend puts a bias on the key map first.
  std::vector<std::string> next = {"p0", "p1", "p2"};
  next.insert(next.end(), keys.begin(), keys.end());
  apply(next, 200);
  measureAll(container);

  // Inserts near the end renumber the rows after them, near the start the rows before them.
  next = keys;
  next.insert(next.end() - 5, {"late0", "late1"});
  apply(next, 203);
  next = keys;
  next.insert(next.begin() + 4, "early");
  apply(next, 205);

  // A remove, a replace, and two rows swapped near each other.
  next = keys;
  next.erase(next.begin() + 100, next.begin() + 103);
  apply(next, 203);
  next = keys;
  next[50] = "swapped-in";
  apply(next, 202);
  next = keys;
  std::swap(next[60], next[63]);
  apply(next, 203);
  measureAll(container);

  // A key from outside the edit inserted again is a duplicate.
  next = keys;
  next.insert(next.begin() + 120, keys[10]);
  CHECK_EQ(Virtualizer::reconcileElements(container, next), static_cast<std::size_t>(203));
  CHECK(container.revision.hasDuplicateKeys);
  CHECK_EQ(container.findElementIndexByKey(keys[10]), static_cast<std::size_t>(10));
  CHECK(!container.revision.elements[120].measured);
}

/*
 * A layout after an edit reflows only from the edit. Removing the one row wider than the
 * window still brings the content width back to the window.
 */
TEST(removing_the_widest_row_near_the_end_narrows_the_content) {
  std::vector<std::string> keys = keysFor(100);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0));
  measureAll(container);
  Virtualizer::updateElementAtIndex(container, 95, {WINDOW_WIDTH * 2.0, 80.0});
  Virtualizer::update(container, inputFor(keys, 0.0));
  CHECK_NEAR(container.revision.totalContainerWidth, WINDOW_WIDTH * 2.0, 0.001);

  std::vector<std::string> next = keys;
  next.erase(next.begin() + 95);
  Virtualizer::update(container, inputFor(next, 0.0));
  CHECK_NEAR(container.revision.totalContainerWidth, WINDOW_WIDTH, 0.001);
  checkRows(container, next);
}

/*
 * Random edits anywhere, with and without duplicate keys, against a plain model: the first
 * copy of a key finds its row and keeps the size of the key's first old row, every other
 * row starts unmeasured.
 */
TEST(reconcile_random_edits_match_a_plain_model) {
  unsigned seed = 11;
  auto next = [&seed](unsigned limit) {
    seed = seed * 1103515245u + 12345u;
    return (seed >> 8) % limit;
  };
  int fresh = 0;
  for (int list = 0; list < 20; ++list) {
    std::vector<std::string> keys = keysFor(60, "r" + std::to_string(list) + "-");
    Container container;
    Virtualizer::update(container, inputFor(keys, 0.0));
    for (int round = 0; round < 30; ++round) {
      measureAll(container);
      std::vector<std::string> nextKeys = keys;
      unsigned edits = 1 + next(6);
      for (unsigned edit = 0; edit < edits; ++edit) {
        unsigned kind = next(5);
        if (kind == 0 && !nextKeys.empty()) {
          nextKeys.erase(nextKeys.begin() + next(static_cast<unsigned>(nextKeys.size())));
        } else if (kind == 1 && nextKeys.size() > 2) {
          std::swap(nextKeys[next(static_cast<unsigned>(nextKeys.size()))], nextKeys[next(static_cast<unsigned>(nextKeys.size()))]);
        } else if (kind == 2 && !nextKeys.empty() && list % 3 == 0) {
          // A duplicate of an existing key.
          nextKeys.insert(nextKeys.begin() + next(static_cast<unsigned>(nextKeys.size())), nextKeys[next(static_cast<unsigned>(nextKeys.size()))]);
        } else {
          nextKeys.insert(nextKeys.begin() + next(static_cast<unsigned>(nextKeys.size() + 1)), "n" + std::to_string(fresh++));
        }
      }
      if (nextKeys.empty()) {
        nextKeys.push_back("n" + std::to_string(fresh++));
      }

      // The model: sizes of each key's first old row.
      std::unordered_map<std::string, std::pair<bool, double>> oldRows;
      for (const Element& element : container.revision.elements) {
        oldRows.emplace(element.key, std::make_pair(element.measured, element.height));
      }
      Virtualizer::reconcileElements(container, nextKeys);

      std::unordered_map<std::string, std::size_t> firstIndex;
      for (std::size_t index = 0; index < nextKeys.size(); ++index) firstIndex.emplace(nextKeys[index], index);
      CHECK_EQ(container.revision.elements.size(), nextKeys.size());
      for (std::size_t index = 0; index < nextKeys.size(); ++index) {
        const Element& element = container.revision.elements[index];
        CHECK_EQ(element.key, nextKeys[index]);
        CHECK_EQ(container.findElementIndexByKey(nextKeys[index]), firstIndex[nextKeys[index]]);
        auto old = oldRows.find(nextKeys[index]);
        bool survivor = firstIndex[nextKeys[index]] == index && old != oldRows.end();
        CHECK_EQ(element.measured, survivor && old->second.first);
        if (survivor && old->second.first) {
          CHECK_NEAR(element.height, old->second.second, 0.0001);
        }
      }
      for (const auto& [key, row] : oldRows) {
        if (firstIndex.find(key) == firstIndex.end()) {
          CHECK_EQ(container.findElementIndexByKey(key), UNDEFINED_INDEX);
        }
      }
      keys = nextKeys;
      Virtualizer::update(container, inputFor(keys, 0.0));
    }
  }
}

/*
 * A hint the host got wrong, the wrong end or count, falls back to comparing the keys.
 */
TEST(reconcile_wrong_edit_hint_falls_back_to_comparing) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0));
  measureAll(container);

  std::vector<std::string> appended = keys;
  appended.push_back("x1");
  appended.push_back("x2");
  CHECK_EQ(Virtualizer::reconcileElements(container, appended, {KeyEditKind::Prepend, 2}), static_cast<std::size_t>(40));
  Virtualizer::update(container, inputFor(appended, 0.0));
  checkRows(container, appended);

  std::vector<std::string> prepended = {"y1", "y2", "y3"};
  prepended.insert(prepended.end(), appended.begin(), appended.end());
  CHECK_EQ(Virtualizer::reconcileElements(container, prepended, {KeyEditKind::Append, 3}), static_cast<std::size_t>(42));
  Virtualizer::update(container, inputFor(prepended, 0.0));
  checkRows(container, prepended);

  std::vector<std::string> trimmed = slice(prepended, 0, prepended.size() - 4);
  CHECK_EQ(Virtualizer::reconcileElements(container, trimmed, {KeyEditKind::TrimStart, 4}), trimmed.size());
  Virtualizer::update(container, inputFor(trimmed, 0.0));
  checkRows(container, trimmed);

  // A right hint takes the same path the comparison would.
  std::vector<std::string> start = slice(trimmed, 5, trimmed.size());
  FrameInput input = inputFor(start, 0.0);
  input.keyEdit = {KeyEditKind::TrimStart, 5};
  Virtualizer::update(container, input);
  checkRows(container, start);
}
