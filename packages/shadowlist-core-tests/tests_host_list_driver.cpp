/*
 * ListDriver tests: the synchronous layout the native lists run, with a host that applies
 * every result right away, like ShadowListKitListView on iOS and Android.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

constexpr double WINDOW_ALONG = 600.0;
constexpr double WINDOW_CROSS = 300.0;

/*
 * Row heights that differ from the 120 estimate and from each other.
 */
double rowHeight(std::size_t index) {
  return 50.0 + static_cast<double>(index % 3) * 10.0;
}

std::vector<std::string> keysFrom(std::size_t first, std::size_t count) {
  std::vector<std::string> keys;
  for (std::size_t index = first; index < first + count; ++index) {
    keys.push_back("k" + std::to_string(index));
  }
  return keys;
}

/*
 * A host that measures rows by key and applies every result the way the native lists do.
 */
struct Host {
  ListDriver driver;
  double offset = 0.0;
  double content = 0.0;
  bool reachedEnd = false;
  bool settling = false;
  std::size_t measures = 0;

  explicit Host(std::size_t count, ListSettings settings = {}) {
    driver.setSettings(settings);
    driver.setMeasureRow([this](std::size_t, const std::string& key, double) {
      ++measures;
      return rowHeight(static_cast<std::size_t>(std::stoul(key.substr(1))));
    });
    driver.setKeys(keysFrom(0, count));
  }

  PassResult layout(bool userScrolled = false) {
    PassInput input;
    input.offset = offset;
    input.windowAlong = WINDOW_ALONG;
    input.windowCross = WINDOW_CROSS;
    input.userScrolled = userScrolled;
    PassResult result = driver.runPasses(input);
    content = result.contentAlong;
    offset = result.offset;
    reachedEnd = reachedEnd || result.reachedEnd;
    settling = result.settling;
    return result;
  }

  /*
   * Lay out again while a correction waits for the next frame, like the settle frames.
   */
  void settle() {
    layout();
    for (int frame = 0; frame < 10 && settling; ++frame) {
      layout();
    }
  }

  void scrollTo(double value) {
    offset = value;
    layout(true);
  }

  /*
   * The first row whose bottom is below the offset, and how far its top sits from the offset.
   */
  std::pair<std::size_t, double> topRow() const {
    for (std::size_t index = 0; index < driver.getRowCount(); ++index) {
      if (driver.getLeadingAt(index) + driver.getExtentAt(index) > offset) {
        return {index, driver.getLeadingAt(index) - offset};
      }
    }
    return {UNDEFINED_INDEX, 0.0};
  }
};

}

TEST(list_driver_first_layout_measures_the_window_only) {
  Host host(1000);
  host.layout();
  auto window = host.driver.getMeasuredRange();
  CHECK(window.has_value());
  CHECK_EQ(window->low, std::size_t{0});
  CHECK(host.measures > 0);
  CHECK(host.measures < 100);
  CHECK_EQ(host.driver.getExtentAt(0), rowHeight(0));
  CHECK_EQ(host.driver.getExtentAt(1), rowHeight(1));
  CHECK_EQ(host.offset, 0.0);
}

TEST(list_driver_layout_inside_the_band_measures_nothing_new) {
  Host host(1000);
  host.layout();
  std::size_t measured = host.measures;
  PassResult result = host.layout();
  CHECK_EQ(host.measures, measured);
  CHECK(!result.settling);
}

TEST(list_driver_rows_are_measured_once_unless_marked) {
  Host host(1000);
  host.layout();
  std::size_t measured = host.measures;
  host.driver.markRemeasure({1, 2, 5000});
  host.layout();
  CHECK_EQ(host.measures, measured + 2);
}

TEST(list_driver_prepend_keeps_the_top_row_in_place) {
  Host host(200);
  host.settle();
  host.scrollTo(1000.0);
  host.settle();
  auto before = host.topRow();
  CHECK(before.first > 0);
  std::string key = host.driver.getKeyAt(static_cast<std::size_t>(before.first));

  std::vector<std::string> inserted = {"k1000", "k1001", "k1002", "k1003", "k1004"};
  host.driver.insertKeys({0, 1, 2, 3, 4}, inserted);
  host.settle();

  std::size_t index = host.driver.indexOfKey(key);
  CHECK_EQ(index, before.first + 5);
  CHECK_NEAR(host.driver.getLeadingAt(index) - host.offset, before.second, 0.01);
}

TEST(list_driver_insert_keys_sorts_and_drops_repeated_indices) {
  Host host(3);
  host.driver.insertKeys({5, 0, 0}, {"end", "first", "repeat"});
  CHECK_EQ(host.driver.getKeyCount(), std::size_t(5));
  CHECK_EQ(host.driver.getKeyAt(0), std::string("first"));
  CHECK_EQ(host.driver.getKeyAt(1), std::string("k0"));
  CHECK_EQ(host.driver.getKeyAt(4), std::string("end"));
}

TEST(list_driver_delete_keys_ignores_repeats_and_out_of_range) {
  Host host(6);
  host.driver.deleteKeys({4, 1, 1, 99});
  CHECK_EQ(host.driver.getKeyCount(), std::size_t(4));
  CHECK_EQ(host.driver.getKeyAt(0), std::string("k0"));
  CHECK_EQ(host.driver.getKeyAt(1), std::string("k2"));
  CHECK_EQ(host.driver.getKeyAt(2), std::string("k3"));
  CHECK_EQ(host.driver.getKeyAt(3), std::string("k5"));
}

/*
 * The driver's keys after a series of random inserts and deletes, few or many places at a
 * time, match the same edits made on a plain list.
 */
TEST(list_driver_key_edits_match_a_plain_list) {
  Host host(50);
  std::vector<std::string> model = keysFrom(0, 50);
  unsigned seed = 7;
  auto next = [&seed](unsigned limit) {
    seed = seed * 1103515245u + 12345u;
    return (seed >> 8) % limit;
  };
  int fresh = 1000;
  for (int round = 0; round < 200; ++round) {
    unsigned places = 1 + next(round % 2 == 0 ? 4 : 20);
    if (next(2) == 0 || model.size() < 30) {
      std::vector<std::size_t> indices;
      std::vector<std::string> keys;
      for (unsigned place = 0; place < places; ++place) {
        indices.push_back(next(static_cast<unsigned>(model.size() + places)));
        keys.push_back("k" + std::to_string(fresh++));
      }
      host.driver.insertKeys(indices, keys);
      // The model applies the same rule: sorted by index, first key of a repeated index.
      std::vector<std::pair<std::size_t, std::string>> pairs;
      for (std::size_t at = 0; at < indices.size(); ++at) pairs.emplace_back(indices[at], keys[at]);
      std::stable_sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
      pairs.erase(std::unique(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) { return a.first == b.first; }),
        pairs.end());
      for (auto& [index, key] : pairs) model.insert(model.begin() + std::min(index, model.size()), key);
    } else {
      std::vector<std::size_t> indices;
      for (unsigned place = 0; place < places; ++place) indices.push_back(next(static_cast<unsigned>(model.size())));
      host.driver.deleteKeys(indices);
      std::sort(indices.begin(), indices.end());
      indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
      for (auto it = indices.rbegin(); it != indices.rend(); ++it) model.erase(model.begin() + *it);
    }
    CHECK_EQ(host.driver.getKeyCount(), model.size());
    for (std::size_t index = 0; index < model.size(); ++index) {
      CHECK_EQ(host.driver.getKeyAt(index), model[index]);
    }
    // The core takes the keys every few edits, with the end edit hint when there was one.
    if (round % 3 == 0) {
      host.layout();
      CHECK_EQ(host.driver.getRowCount(), model.size());
      for (std::size_t index = 0; index < model.size(); ++index) {
        CHECK_EQ(host.driver.indexOfKey(model[index]), index);
      }
    }
  }
  host.settle();
  CHECK_EQ(host.driver.getRowCount(), model.size());
}

/*
 * Single edits at the ends reach the core as one: prepends, appends and trims each keep the
 * top row in place through the hinted reconcile.
 */
TEST(list_driver_end_edits_keep_the_top_row) {
  Host host(300);
  host.settle();
  host.scrollTo(4000.0);
  host.settle();
  int fresh = 1000;
  for (int round = 0; round < 12; ++round) {
    auto before = host.topRow();
    std::string key = host.driver.getKeyAt(static_cast<std::size_t>(before.first));
    std::size_t count = host.driver.getKeyCount();
    switch (round % 6) {
      case 4: {
        // reloadData on a native list splices the changed middle, here an append.
        host.driver.replaceKeys(count, 0, {"k" + std::to_string(fresh)});
        fresh += 1;
        break;
      }
      case 5:
        host.driver.replaceKeys(0, 2, {});
        break;
      case 0:
        host.driver.insertKeys({0, 1, 2}, {"k" + std::to_string(fresh), "k" + std::to_string(fresh + 1), "k" + std::to_string(fresh + 2)});
        fresh += 3;
        break;
      case 1:
        host.driver.insertKeys({count, count + 1}, {"k" + std::to_string(fresh), "k" + std::to_string(fresh + 1)});
        fresh += 2;
        break;
      case 2:
        host.driver.deleteKeys({0, 1});
        break;
      default:
        host.driver.deleteKeys({count - 1, count - 2, count - 3});
        break;
    }
    host.settle();
    std::size_t index = host.driver.indexOfKey(key);
    CHECK(index != UNDEFINED_INDEX);
    CHECK_NEAR(host.driver.getLeadingAt(index) - host.offset, before.second, 0.01);
    for (std::size_t row = 0; row < host.driver.getKeyCount(); ++row) {
      CHECK_EQ(host.driver.indexOfKey(host.driver.getKeyAt(row)), row);
    }
  }
}

TEST(list_driver_reload_keys_applies_only_the_changed_middle) {
  Host host(8);
  host.settle();
  std::vector<std::string> next = {"k0", "k1", "k90", "k4", "k5", "k6", "k7"};
  host.driver.reloadKeys(next);
  CHECK_EQ(host.driver.getKeyCount(), next.size());
  for (std::size_t index = 0; index < next.size(); ++index) CHECK_EQ(host.driver.getKeyAt(index), next[index]);

  // The same keys again change nothing the core would see.
  host.settle();
  std::uint64_t version = host.driver.getGeometryVersion();
  host.driver.reloadKeys(next);
  host.settle();
  CHECK_EQ(host.driver.getGeometryVersion(), version);

  // Keys repeated at both ends still give the right list.
  host.driver.reloadKeys({"k0", "k0", "k7"});
  CHECK_EQ(host.driver.getKeyCount(), std::size_t(3));
  CHECK_EQ(host.driver.getKeyAt(1), std::string("k0"));
  host.driver.reloadKeys({});
  CHECK_EQ(host.driver.getKeyCount(), std::size_t(0));
}

TEST(list_driver_replace_keys_splices_the_range) {
  Host host(6);
  host.driver.replaceKeys(1, 2, {"a", "b", "c"});
  std::vector<std::string> expected = {"k0", "a", "b", "c", "k3", "k4", "k5"};
  CHECK_EQ(host.driver.getKeyCount(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) CHECK_EQ(host.driver.getKeyAt(index), expected[index]);

  host.driver.replaceKeys(0, 4, {"z"});
  expected = {"z", "k3", "k4", "k5"};
  CHECK_EQ(host.driver.getKeyCount(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) CHECK_EQ(host.driver.getKeyAt(index), expected[index]);

  // Past the end appends, and a count past the end is clipped.
  host.driver.replaceKeys(99, 5, {"end"});
  host.driver.replaceKeys(3, 99, {});
  expected = {"z", "k3", "k4"};
  CHECK_EQ(host.driver.getKeyCount(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) CHECK_EQ(host.driver.getKeyAt(index), expected[index]);
}

TEST(list_driver_scroll_to_index_lands_on_the_row) {
  Host host(1000);
  host.settle();
  host.driver.scrollToRow(500, 0.0);
  host.settle();
  CHECK(!host.settling);
  CHECK_NEAR(host.driver.getLeadingAt(500), host.offset, 0.01);
  CHECK_EQ(host.driver.getExtentAt(500), rowHeight(500));
}

TEST(list_driver_scroll_to_end_reaches_the_end) {
  Host host(1000);
  host.settle();
  host.driver.scrollToEnd();
  host.settle();
  CHECK_NEAR(host.offset, host.content - WINDOW_ALONG, 0.01);
  CHECK(host.reachedEnd);
}

TEST(list_driver_landing_sends_its_command_once) {
  Host host(1000);
  host.settle();
  host.scrollTo(3000.0);
  host.settle();
  host.driver.setLanding({ScrollLanding::Target::Start, 0, 0.0});
  CHECK(host.driver.land());
  CHECK(!host.driver.land());
  host.settle();
  CHECK_EQ(host.offset, 0.0);
}

TEST(list_driver_cancelled_landing_does_nothing) {
  Host host(100);
  host.settle();
  host.driver.setLanding({ScrollLanding::Target::Row, 50, 0.0});
  host.driver.cancelLanding();
  CHECK(!host.driver.land());
}

TEST(list_driver_animated_target_is_inside_the_range) {
  Host host(100);
  host.settle();
  double maxOffset = host.content - WINDOW_ALONG;
  CHECK_EQ(host.driver.animatedTargetOffset(10, 0.0, WINDOW_ALONG, maxOffset), host.driver.getLeadingAt(10));
  CHECK_EQ(host.driver.animatedTargetOffset(99, 0.0, WINDOW_ALONG, maxOffset), maxOffset);
}

TEST(list_driver_sticky_header_pins_and_is_pushed_up) {
  Host host(300);
  host.driver.setStickyIndices({40, 0, 20});
  host.settle();
  host.scrollTo(host.driver.getLeadingAt(20) + 10.0);
  host.settle();
  CHECK_EQ(host.driver.activeStickyIndex(host.offset), std::size_t{20});
  CHECK_EQ(host.driver.stickyLeading(20, host.offset), host.offset);

  // Just above header 40, which pushes 20 up by the overlap.
  double next = host.driver.getLeadingAt(40);
  double pushed = host.driver.stickyLeading(20, next - 5.0);
  CHECK_NEAR(pushed, next - host.driver.getExtentAt(20), 0.01);
}

/*
 * The pinning ShadowListKitListView.layoutSticky on Android runs over the copied sticky frames: the last
 * frame starting at or above the offset, pushed up by the next placed one. Rows not placed yet
 * have an infinite leading edge and never pin or push.
 */
TEST(list_driver_sticky_frames_pin_the_same_header_as_the_driver) {
  Host host(300);
  host.driver.setStickyIndices({0, 20, 40});
  host.settle();
  // Keys for two more sticky rows that the core places on the next layout only.
  std::vector<std::string> keys = keysFrom(0, 400);
  host.driver.setKeys(keys);
  host.driver.setStickyIndices({0, 20, 40, 320, 350});
  std::vector<double> frames;
  host.driver.stickyFrames(frames);
  CHECK_EQ(frames.size(), std::size_t{10});
  CHECK(std::isinf(frames[6]) && frames[6] > 0.0);
  CHECK(std::isinf(frames[8]) && frames[8] > 0.0);

  std::vector<std::size_t> rows = {0, 20, 40, 320, 350};
  auto leadingAt = [&](std::size_t at) { return frames[at * 2]; };
  for (double offset : {0.0, 500.0, host.driver.getLeadingAt(20) + 3.0, host.driver.getLeadingAt(40) - 2.0, 1e9}) {
    std::size_t position = pinnedSectionIndex(rows.size(), offset, leadingAt);
    std::size_t active = host.driver.activeStickyIndex(offset);
    CHECK_EQ(position == UNDEFINED_INDEX ? UNDEFINED_INDEX : rows[position], active);
    if (position == UNDEFINED_INDEX) {
      continue;
    }
    bool hasNext = position + 1 < rows.size() && std::isfinite(frames[(position + 1) * 2]);
    double pinned = pinnedSectionLeading(frames[position * 2], frames[position * 2 + 1], offset, hasNext,
      hasNext ? frames[(position + 1) * 2] : 0.0);
    CHECK_NEAR(pinned, host.driver.stickyLeading(active, offset), 1e-9);
  }
}

TEST(list_driver_pinned_header_far_above_the_window_is_measured) {
  Host host(1000);
  host.driver.setStickyIndices({0});
  host.settle();
  host.scrollTo(20000.0);
  host.settle();
  CHECK_EQ(host.driver.activeStickyIndex(host.offset), std::size_t{0});
  CHECK_EQ(host.driver.getExtentAt(0), rowHeight(0));
  MountPlan plan = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  CHECK_EQ(plan.sticky, std::size_t{0});
  CHECK(plan.low != UNDEFINED_INDEX && plan.low > 0);
}

TEST(list_driver_mount_plan_covers_the_viewport) {
  Host host(1000);
  host.settle();
  host.scrollTo(2000.0);
  host.settle();
  MountPlan plan = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  CHECK(plan.low != UNDEFINED_INDEX);
  CHECK(host.driver.getLeadingAt(plan.low) <= host.offset);
  double bottom = host.driver.getLeadingAt(plan.high) + host.driver.getExtentAt(plan.high);
  CHECK(bottom >= host.offset + WINDOW_ALONG);
  CHECK(host.driver.shouldMount(plan, plan.low));
}

/*
 * The mount rule ShadowListKitListView.mountCells on Android runs over its copied frames: the view spans
 * the window, the pad on both ends and the insets before and after it.
 */
TEST(list_driver_mount_plan_reaches_into_the_insets) {
  Host host(1000);
  host.settle();
  host.scrollTo(2000.0);
  host.settle();
  constexpr double LEADING_INSET = 100.0;
  constexpr double TRAILING_INSET = 80.0;
  MountPlan bare = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  MountPlan inset = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0, LEADING_INSET, TRAILING_INSET);
  CHECK_NEAR(inset.viewLow, host.offset - LEADING_INSET, 1e-9);
  CHECK_NEAR(inset.viewHigh, host.offset + WINDOW_ALONG + TRAILING_INSET, 1e-9);
  CHECK(inset.low < bare.low);
  CHECK(inset.high > bare.high);
  CHECK(host.driver.getLeadingAt(inset.low) <= host.offset - LEADING_INSET);
  double bottom = host.driver.getLeadingAt(inset.high) + host.driver.getExtentAt(inset.high);
  CHECK(bottom >= host.offset + WINDOW_ALONG + TRAILING_INSET);

  // Pad and insets add up on each end.
  MountPlan padded = host.driver.planMount(host.offset, WINDOW_ALONG, 30.0, LEADING_INSET, TRAILING_INSET);
  CHECK_NEAR(padded.viewLow, host.offset - 30.0 - LEADING_INSET, 1e-9);
  CHECK_NEAR(padded.viewHigh, host.offset + WINDOW_ALONG + 30.0 + TRAILING_INSET, 1e-9);
}

TEST(list_driver_reset_keeps_the_first_visible_row) {
  Host host(1000);
  host.settle();
  host.scrollTo(5000.0);
  host.settle();
  auto before = host.topRow();
  CHECK_EQ(host.driver.getVisibleRange()->low, before.first);
  host.driver.resetKeepingPosition();
  host.settle();
  auto after = host.topRow();
  CHECK_EQ(after.first, before.first);
  CHECK_NEAR(after.second, before.second, 0.01);
}

TEST(list_driver_visible_range_is_the_rows_on_screen) {
  Host host(1000);
  host.settle();
  host.scrollTo(5000.0);
  host.settle();
  auto visible = host.driver.getVisibleRange();
  CHECK(visible.has_value());
  CHECK_EQ(visible->low, host.topRow().first);
  CHECK(host.driver.getLeadingAt(visible->high) < host.offset + WINDOW_ALONG);
  CHECK(host.driver.getLeadingAt(visible->high) + host.driver.getExtentAt(visible->high) >= host.offset + WINDOW_ALONG);
  auto window = host.driver.getMeasuredRange();
  CHECK(window->low < visible->low);
}

TEST(list_driver_reset_keeps_an_inverted_list_at_its_end) {
  ListSettings settings;
  settings.inverted = true;
  Host host(1000, settings);
  host.settle();
  CHECK_NEAR(host.offset, host.content - WINDOW_ALONG, 0.5);
  host.driver.resetKeepingPosition();
  host.settle();
  CHECK_NEAR(host.offset, host.content - WINDOW_ALONG, 0.5);
}

TEST(list_driver_row_rect_spans_the_cross_axis) {
  Host host(10);
  host.settle();
  RowRect rect = host.driver.getRowRect(2);
  CHECK_EQ(rect.x, 0.0);
  CHECK_EQ(rect.y, host.driver.getLeadingAt(2));
  CHECK_EQ(rect.width, WINDOW_CROSS);
  CHECK_EQ(rect.height, rowHeight(2));
}

TEST(list_driver_drag_moves_the_drop_slot_and_skips_the_held_row) {
  Host host(100);
  host.settle();
  double grab = host.driver.getLeadingAt(2) + 10.0;
  host.driver.dragBegin(2, grab, 0.0);
  CHECK(host.driver.isDragging());
  CHECK_EQ(host.driver.getHeldIndex(), std::size_t{2});

  // Past the middle of row 4.
  double touch = host.driver.getLeadingAt(4) + host.driver.getExtentAt(4) * 0.5 + 15.0;
  DragOffset placed = host.driver.placeHeld(2, touch, 0.0);
  CHECK_NEAR(placed.leading, touch - grab, 0.01);
  host.driver.dragUpdateInsertion({0, 1, 2, 3, 4, 5, 6});
  CHECK_EQ(host.driver.getDragOriginIndex(), std::size_t{2});
  CHECK_EQ(host.driver.getDragInsertionIndex(), std::size_t{4});
  CHECK_EQ(host.driver.dragShiftFor(3).leading, -host.driver.getExtentAt(2));
  CHECK_EQ(host.driver.dragShiftFor(5).leading, 0.0);

  host.driver.dragEnd();
  CHECK(!host.driver.isDragging());
  CHECK_EQ(host.driver.getHeldIndex(), UNDEFINED_INDEX);
}

TEST(list_driver_held_row_follows_its_key) {
  Host host(20);
  host.settle();
  host.driver.dragBegin(3, host.driver.getLeadingAt(3), 0.0);
  host.driver.insertKeys({0}, {"k500"});
  host.settle();
  CHECK_EQ(host.driver.getHeldIndex(), std::size_t{4});
  host.driver.deleteKeys({4});
  host.settle();
  CHECK_EQ(host.driver.getHeldIndex(), UNDEFINED_INDEX);
}

TEST(container_has_pending_command_while_a_scroll_command_runs) {
  Container probe;
  CHECK(!probe.hasPendingCommand());
  probe.scrollToEnd();
  CHECK(probe.hasPendingCommand());
}

TEST(list_driver_anchor_restores_the_row_partway_in) {
  Host host(1000);
  host.settle();
  host.scrollTo(3000.0);
  host.settle();
  auto anchor = host.driver.getAnchorState(host.offset);
  CHECK(anchor.has_value());
  CHECK(anchor->offset >= 0.0);
  std::size_t row = host.driver.indexOfKey(anchor->key);
  CHECK_NEAR(host.driver.getLeadingAt(row) + anchor->offset, host.offset, 0.01);

  // A new list with rows inserted above lands the same row the same distance in.
  Host restored(1000);
  restored.driver.insertKeys({0, 1, 2}, {"k5000", "k5001", "k5002"});
  restored.settle();
  CHECK(restored.driver.restoreAnchorState(*anchor));
  restored.settle();
  std::size_t index = restored.driver.indexOfKey(anchor->key);
  CHECK_EQ(index, row + 3);
  CHECK_NEAR(restored.offset - restored.driver.getLeadingAt(index), anchor->offset, 0.01);
  CHECK(!restored.driver.restoreAnchorState({"missing", 0.0}));
}

TEST(list_driver_prefetches_window_rows_once_and_cancels_them_when_they_leave) {
  Host host(1000);
  host.settle();
  std::vector<std::size_t> prefetch;
  std::vector<std::size_t> cancel;
  MountPlan plan = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  host.driver.updatePrefetch(plan.low, plan.high, prefetch, cancel);
  CHECK(!prefetch.empty());
  CHECK(cancel.empty());
  for (std::size_t index : prefetch) CHECK(index > plan.high);

  // The same frame again prefetches nothing new.
  host.driver.updatePrefetch(plan.low, plan.high, prefetch, cancel);
  CHECK(prefetch.empty());

  // A jump far away cancels the rows that never showed.
  host.scrollTo(30000.0);
  host.settle();
  plan = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  host.driver.updatePrefetch(plan.low, plan.high, prefetch, cancel);
  CHECK(!cancel.empty());
  CHECK(!prefetch.empty());
}

TEST(list_driver_moved_key_keeps_its_size_and_the_top_row_holds) {
  Host host(300);
  host.settle();
  host.scrollTo(4000.0);
  host.settle();
  auto before = host.topRow();
  std::string top = host.driver.getKeyAt(before.first);
  // Move the last row above the viewport through the batch plan, like moveItem.
  std::vector<std::string> keys;
  for (std::size_t index = 0; index < host.driver.getKeyCount(); ++index) keys.push_back(host.driver.getKeyAt(index));
  BatchUpdate batch;
  batch.moved = {{keys.size() - 1, 2}};
  auto plan = planBatch(keys.size(), keys.size(), batch);
  CHECK(plan.has_value());
  std::vector<std::string> next;
  for (std::size_t source : plan->sources) next.push_back(keys[source]);
  std::string moved = keys.back();
  host.driver.reloadKeys(next);
  host.settle();
  std::size_t index = host.driver.indexOfKey(top);
  CHECK_EQ(index, before.first + 1);
  CHECK_NEAR(host.driver.getLeadingAt(index) - host.offset, before.second, 0.01);
  CHECK_EQ(host.driver.indexOfKey(moved), std::size_t(2));
}
