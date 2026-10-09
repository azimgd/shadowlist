/*
 * KeyDiff tests: the key diff and the batch plan both native lists use.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/KeyDiff.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * Apply a diff to previous the way a list would: delete, then put inserts and moves in place.
 */
std::vector<std::string> applyDiff(
  const std::vector<std::string>& previous,
  const std::vector<std::string>& next,
  const KeyDiff& diff) {
  BatchUpdate batch;
  batch.deleted = diff.deleted;
  batch.inserted = diff.inserted;
  batch.moved = diff.moved;
  auto plan = planBatch(previous.size(), next.size(), batch);
  std::vector<std::string> result;
  if (!plan) {
    return result;
  }
  for (std::size_t index = 0; index < plan->sources.size(); ++index) {
    std::size_t source = plan->sources[index];
    result.push_back(source == UNDEFINED_INDEX ? next[index] : previous[source]);
  }
  return result;
}

}

TEST(key_diff_of_equal_lists_is_empty) {
  std::vector<std::string> keys = {"a", "b", "c"};
  CHECK(diffKeys(keys, keys).isEmpty());
}

TEST(key_diff_finds_inserts_deletes_and_the_fewest_moves) {
  std::vector<std::string> previous = {"a", "b", "c", "d", "e"};
  std::vector<std::string> next = {"x", "a", "d", "c", "e"};
  KeyDiff diff = diffKeys(previous, next);
  CHECK_EQ(diff.inserted.size(), std::size_t(1));
  CHECK_EQ(diff.inserted[0], std::size_t(0));
  CHECK_EQ(diff.deleted.size(), std::size_t(1));
  CHECK_EQ(diff.deleted[0], std::size_t(1));
  CHECK_EQ(diff.moved.size(), std::size_t(1));
  std::vector<std::string> applied = applyDiff(previous, next, diff);
  CHECK_EQ(applied.size(), next.size());
  for (std::size_t index = 0; index < next.size(); ++index) CHECK_EQ(applied[index], next[index]);
}

TEST(key_diff_counts_a_repeated_key_once) {
  std::vector<std::string> previous = {"a", "a", "b"};
  std::vector<std::string> next = {"b", "a", "a"};
  KeyDiff diff = diffKeys(previous, next);
  std::vector<std::string> applied = applyDiff(previous, next, diff);
  CHECK_EQ(applied.size(), next.size());
  for (std::size_t index = 0; index < next.size(); ++index) CHECK_EQ(applied[index], next[index]);
}

/*
 * Random shuffles, inserts and deletes: the diff always rebuilds the next list.
 */
TEST(key_diff_rebuilds_random_changes) {
  unsigned seed = 11;
  auto next = [&seed](unsigned limit) {
    seed = seed * 1103515245u + 12345u;
    return (seed >> 8) % limit;
  };
  int fresh = 0;
  for (int round = 0; round < 200; ++round) {
    std::vector<std::string> previous;
    unsigned count = next(30);
    for (unsigned at = 0; at < count; ++at) previous.push_back("k" + std::to_string(fresh++));
    std::vector<std::string> changed = previous;
    for (unsigned edit = next(8); edit > 0; --edit) {
      unsigned kind = next(3);
      if (kind == 0 || changed.empty()) {
        changed.insert(changed.begin() + next(static_cast<unsigned>(changed.size() + 1)), "n" + std::to_string(fresh++));
      } else if (kind == 1) {
        changed.erase(changed.begin() + next(static_cast<unsigned>(changed.size())));
      } else {
        unsigned from = next(static_cast<unsigned>(changed.size()));
        std::string key = changed[from];
        changed.erase(changed.begin() + from);
        changed.insert(changed.begin() + next(static_cast<unsigned>(changed.size() + 1)), key);
      }
    }
    KeyDiff diff = diffKeys(previous, changed);
    std::vector<std::string> applied = applyDiff(previous, changed, diff);
    CHECK_EQ(applied.size(), changed.size());
    for (std::size_t index = 0; index < changed.size() && index < applied.size(); ++index) {
      CHECK_EQ(applied[index], changed[index]);
    }
  }
}

TEST(batch_plan_applies_deletes_inserts_and_moves_like_uikit) {
  // a b c d e: delete b, move d to the front, insert x at 2 of the next data.
  BatchUpdate batch;
  batch.deleted = {1};
  batch.moved = {{3, 0}};
  batch.inserted = {2};
  auto plan = planBatch(5, 5, batch);
  CHECK(plan.has_value());
  std::vector<std::size_t> expected = {3, 0, UNDEFINED_INDEX, 2, 4};
  CHECK_EQ(plan->sources.size(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) CHECK_EQ(plan->sources[index], expected[index]);
}

TEST(batch_plan_rejects_a_batch_that_does_not_add_up) {
  BatchUpdate batch;
  batch.inserted = {0};
  CHECK(!planBatch(3, 3, batch).has_value());
  BatchUpdate twice;
  twice.deleted = {1};
  twice.moved = {{1, 0}};
  CHECK(!planBatch(3, 2, twice).has_value());
  BatchUpdate clash;
  clash.inserted = {0};
  clash.moved = {{2, 0}};
  CHECK(!planBatch(3, 4, clash).has_value());
  BatchUpdate outside;
  outside.deleted = {7};
  CHECK(!planBatch(3, 2, outside).has_value());
}
