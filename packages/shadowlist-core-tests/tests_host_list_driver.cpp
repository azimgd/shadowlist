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
  bool prefetch = false;
  bool tracking = false;
  bool clampStart = false;
  bool clampEnd = false;
  double clampSlack = 0.0;
  std::size_t measures = 0;

  /*
   * With a size here every row measures the same, which leaves the content as it is wherever
   * the list goes.
   */
  double uniformSize = 0.0;

  explicit Host(std::size_t count, ListSettings settings = {}) {
    driver.setSettings(settings);
    driver.setMeasureRow([this](std::size_t, const std::string& key, double) {
      ++measures;
      return uniformSize > 0.0 ? uniformSize : rowHeight(static_cast<std::size_t>(std::stoul(key.substr(1))));
    });
    driver.setKeys(keysFrom(0, count));
  }

  PassResult layout(bool userScrolled = false) {
    PassInput input;
    input.offset = offset;
    input.windowAlong = WINDOW_ALONG;
    input.windowCross = WINDOW_CROSS;
    input.userScrolled = userScrolled;
    input.prefetch = prefetch;
    input.tracking = tracking;
    input.clampStart = clampStart;
    input.clampEnd = clampEnd;
    input.clampSlack = clampSlack;
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

TEST(list_driver_first_layout_measures_only_the_rows_near_the_viewport) {
  Host host(1000);
  host.layout();
  auto measured = host.driver.getMeasuredRange();
  CHECK(measured.has_value());
  CHECK_EQ(measured->low, std::size_t{0});
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

TEST(list_driver_one_layout_measures_every_row_its_measured_range_ends_with) {
  // One tall row sets the estimate of the short rows that follow it.
  Host host(1);
  host.driver.setMeasureRow([&host](std::size_t, const std::string& key, double) {
    ++host.measures;
    return key == "k0" ? 560.0 : 20.0;
  });
  host.layout();
  host.driver.setKeys(keysFrom(0, 400));
  PassResult result = host.layout();
  CHECK(!result.settling);
  auto measured = host.driver.getMeasuredRange();
  CHECK(measured.has_value());
  // The rows up to an overscan past the viewport: the tall one and 33 short ones.
  CHECK_EQ(measured->high, std::size_t{33});
  CHECK_EQ(host.measures, std::size_t{34});
  host.layout();
  CHECK_EQ(host.measures, std::size_t{34});
}

TEST(list_driver_a_user_move_reported_at_rest_ends_in_the_same_layout) {
  ListSettings settings;
  settings.inverted = true;
  settings.followAppends = true;
  Host host(60, settings);
  host.settle();
  double end = host.content - WINDOW_ALONG;
  CHECK_NEAR(host.offset, end, 0.01);
  // A few points up, inside the band where the list still rests on its newest row.
  host.scrollTo(end - 10.0);
  CHECK_NEAR(host.offset, end, 0.01);
  host.layout();
  CHECK_NEAR(host.offset, end, 0.01);
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

/*
 * A scroll to a row arrives on the sizes its pass began with. The rows that pass then measures
 * move the row in its track, and the layout still ends where the command aims.
 */
TEST(list_driver_scroll_to_a_row_lands_on_the_sizes_its_layout_measured) {
  ListSettings settings;
  settings.numberOfColumns = 3;
  Host host(114, settings);
  // Mostly short rows, some tall and a few taller than the window.
  host.driver.setMeasureRow([](std::size_t index, const std::string&, double) {
    unsigned hash = (static_cast<unsigned>(index) + 1) * 2654435761u * 37u;
    unsigned bucket = (hash >> 8) % 10;
    unsigned step = (hash >> 16) % 10;
    if (bucket < 6) {
      return 80.0 + step * 24.0;
    }
    return bucket < 9 ? 240.0 + step * 60.0 : 800.0 + step * 120.0;
  });
  host.settle();
  host.scrollTo(host.content / 6.0);
  host.settle();
  for (std::size_t row : {std::size_t{111}, std::size_t{108}}) {
    host.driver.scrollToRow(row, 0.5);
    host.layout();
    CHECK(!host.settling);
    // The rows are in the last window, where the end of the content cuts the view position short.
    double room = WINDOW_ALONG - host.driver.getExtentAt(row);
    double end = host.content - WINDOW_ALONG;
    CHECK(room > 0.0);
    CHECK_NEAR(host.offset, std::min(host.driver.getLeadingAt(row) - 0.5 * room, end), 0.01);
    host.scrollTo(host.content / 6.0);
    host.settle();
  }
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

TEST(list_driver_pass_band_ends_where_another_header_pins) {
  Host host(300);
  host.driver.setStickyIndices({0, 20, 40});
  host.settle();
  host.scrollTo(host.driver.getLeadingAt(40) + 100.0);
  // Just past the header at row 20: the band holds no offset where row 0 pins.
  host.scrollTo(host.driver.getLeadingAt(20) + 1.0);
  PassResult result = host.layout();
  CHECK_EQ(host.driver.activeStickyIndex(host.offset), std::size_t{20});
  CHECK(result.band.contains(host.offset));
  CHECK(result.band.low >= host.driver.getLeadingAt(20));
  // Just before the header at row 40: the band holds no offset where it pins.
  host.scrollTo(host.driver.getLeadingAt(40) - 1.0);
  result = host.layout();
  CHECK_EQ(host.driver.activeStickyIndex(host.offset), std::size_t{20});
  CHECK(result.band.contains(host.offset));
  CHECK(result.band.high < host.driver.getLeadingAt(40));
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

TEST(list_driver_pinned_header_far_above_the_viewport_is_measured) {
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

/*
 * A host moving the offset inside the plan's band mounts the same rows without planning again.
 * Just past either end a row enters or leaves.
 */
TEST(list_driver_mount_plan_band_holds_the_same_rows) {
  for (std::size_t columns : {std::size_t{1}, std::size_t{3}}) {
    ListSettings settings;
    settings.numberOfColumns = columns;
    Host host(1000, settings);
    host.settle();
    host.scrollTo(2000.0);
    host.settle();
    constexpr double PAD = 40.0;
    MountPlan plan = host.driver.planMount(host.offset, WINDOW_ALONG, PAD, 10.0, 20.0);
    CHECK(plan.band.contains(host.offset));
    CHECK(plan.band.low < plan.band.high);
    auto mounted = [&](const MountPlan& at) {
      std::vector<std::size_t> rows;
      for (std::size_t index = at.low; at.low != UNDEFINED_INDEX && index <= at.high; ++index) {
        if (host.driver.shouldMount(at, index)) {
          rows.push_back(index);
        }
      }
      return rows;
    };
    std::vector<std::size_t> rows = mounted(plan);
    for (double offset : {plan.band.low, (plan.band.low + plan.band.high) / 2, plan.band.high}) {
      CHECK(mounted(host.driver.planMount(offset, WINDOW_ALONG, PAD, 10.0, 20.0)) == rows);
    }
    CHECK(mounted(host.driver.planMount(plan.band.low - 0.01, WINDOW_ALONG, PAD, 10.0, 20.0)) != rows);
    CHECK(mounted(host.driver.planMount(plan.band.high + 0.01, WINDOW_ALONG, PAD, 10.0, 20.0)) != rows);
  }
}

/*
 * The pinned row changes with the offset, and a plan with sticky rows holds only its own offset.
 */
TEST(list_driver_mount_plan_band_with_sticky_rows_is_its_offset) {
  Host host(300);
  host.driver.setStickyIndices({0, 20, 40});
  host.settle();
  host.scrollTo(1500.0);
  host.settle();
  MountPlan plan = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  CHECK_EQ(plan.band.low, host.offset);
  CHECK_EQ(plan.band.high, host.offset);
}

/*
 * What a mount pass over a plan covers, with nothing else moved.
 */
MountRecord recordOf(const MountPlan& plan, const MountInputs& inputs) {
  MountRecord record;
  record.low = plan.low;
  record.high = plan.high;
  record.sticky = plan.sticky;
  record.band = plan.band;
  record.inputs = inputs;
  return record;
}

TEST(a_mount_pass_over_the_same_rows_of_one_column_is_skipped) {
  Host host(1000);
  host.settle();
  host.scrollTo(2000.0);
  host.settle();
  MountInputs inputs{WINDOW_ALONG, 40.0, 0.0, 0.0};
  MountPlan plan = host.driver.planMount(host.offset, inputs.windowAlong, inputs.pad);
  MountRecord mounted = recordOf(plan, inputs);
  CHECK(!shouldSkipMount(MountRecord(), mounted, host.offset, 1, false));
  CHECK(shouldSkipMount(mounted, mounted, host.offset, 1, false));
  // The ends alone decide: an offset outside the band or new insets keep the rows of one column.
  MountRecord moved = mounted;
  moved.inputs.trailingInset = 1.0;
  CHECK(shouldSkipMount(mounted, moved, plan.band.high + 100.0, 1, false));
  // A row entered at either end.
  MountPlan further = host.driver.planMount(plan.band.high + 1.0, inputs.windowAlong, inputs.pad);
  CHECK(further.low != plan.low || further.high != plan.high);
  CHECK(!shouldSkipMount(mounted, recordOf(further, inputs), plan.band.high + 1.0, 1, false));
  // The geometry, the data and the shift each mount again, and so does a held row.
  MountRecord changed = mounted;
  changed.geometry = 1;
  CHECK(!shouldSkipMount(mounted, changed, host.offset, 1, false));
  changed = mounted;
  changed.structure = 1;
  CHECK(!shouldSkipMount(mounted, changed, host.offset, 1, false));
  changed = mounted;
  changed.shift = 0.25;
  CHECK(!shouldSkipMount(mounted, changed, host.offset, 1, false));
  CHECK(!shouldSkipMount(mounted, mounted, host.offset, 1, true));
}

/*
 * In a grid whose columns differ a row between the plan's ends comes into view while the ends
 * stay. The pass is skipped only inside the band of the plan before.
 */
TEST(a_mount_pass_in_a_grid_is_skipped_only_inside_the_band_of_the_plan_before) {
  ListSettings settings;
  settings.numberOfColumns = 3;
  Host host(1000, settings);
  host.settle();
  host.scrollTo(2000.0);
  host.settle();
  MountInputs inputs{WINDOW_ALONG, 0.0, 0.0, 0.0};
  auto planAt = [&](double offset) { return host.driver.planMount(offset, inputs.windowAlong, inputs.pad); };
  auto rowsOf = [&](const MountPlan& plan) {
    std::vector<std::size_t> rows;
    for (std::size_t index = plan.low; plan.low != UNDEFINED_INDEX && index <= plan.high; ++index) {
      if (host.driver.shouldMount(plan, index)) {
        rows.push_back(index);
      }
    }
    return rows;
  };
  MountPlan plan = planAt(host.offset);
  MountRecord mounted = recordOf(plan, inputs);
  CHECK(shouldSkipMount(mounted, recordOf(planAt(plan.band.low), inputs), plan.band.low, 3, false));
  CHECK(shouldSkipMount(mounted, recordOf(planAt(plan.band.high), inputs), plan.band.high, 3, false));
  // Walk on until the ends stay from one offset to the next and a row between them leaves.
  bool found = false;
  for (double offset = host.offset + 0.5; offset < host.offset + 2000.0 && !found; offset += 0.5) {
    MountPlan next = planAt(offset);
    found = next.low == plan.low && next.high == plan.high && rowsOf(next) != rowsOf(plan);
    if (found) {
      CHECK(!plan.band.contains(offset));
      CHECK(!shouldSkipMount(mounted, recordOf(next, inputs), offset, 3, false));
      // One column's rule would skip the pass and leave the rows as they were.
      CHECK(shouldSkipMount(mounted, recordOf(next, inputs), offset, 1, false));
    }
    plan = next;
    mounted = recordOf(plan, inputs);
  }
  CHECK(found);
}

/*
 * A band holds for the window, the padding and the insets its plan was made with. A window that
 * grew shows rows the band knows nothing of.
 */
TEST(a_mount_pass_in_a_grid_is_not_skipped_once_the_window_or_an_inset_changed) {
  ListSettings settings;
  settings.numberOfColumns = 3;
  Host host(1000, settings);
  host.settle();
  MountInputs inputs{WINDOW_ALONG, 0.0, 0.0, 0.0};
  MountPlan plan = host.driver.planMount(host.offset, inputs.windowAlong, inputs.pad);
  MountRecord mounted = recordOf(plan, inputs);
  for (int field = 0; field < 4; ++field) {
    MountRecord next = mounted;
    double& value = field == 0 ? next.inputs.windowAlong : field == 1 ? next.inputs.pad
      : field == 2 ? next.inputs.leadingInset : next.inputs.trailingInset;
    value += 1.0;
    CHECK(!shouldSkipMount(mounted, next, host.offset, 3, false));
    CHECK(shouldSkipMount(mounted, next, host.offset, 1, false));
  }
  // The band a skipped pass keeps is its own plan's: the pass after it compares against that one.
  MountRecord grown = mounted;
  grown.inputs.windowAlong += 1.0;
  CHECK(shouldSkipMount(grown, grown, host.offset, 3, false));
}

TEST(a_mount_pass_is_not_skipped_when_another_header_pins) {
  Host host(300);
  host.driver.setStickyIndices({0, 20, 40});
  host.settle();
  host.scrollTo(host.driver.getLeadingAt(20) - 10.0);
  host.settle();
  MountInputs inputs{WINDOW_ALONG, 0.0, 0.0, 0.0};
  MountPlan before = host.driver.planMount(host.offset, inputs.windowAlong, inputs.pad);
  double pinned = host.driver.getLeadingAt(20) + 1.0;
  MountPlan after = host.driver.planMount(pinned, inputs.windowAlong, inputs.pad);
  CHECK_EQ(before.sticky, std::size_t{0});
  CHECK_EQ(after.sticky, std::size_t{20});
  MountRecord next = recordOf(after, inputs);
  // Only the pinned header differs.
  next.low = before.low;
  next.high = before.high;
  CHECK(!shouldSkipMount(recordOf(before, inputs), next, pinned, 1, false));
  // A list with sticky rows moves under the same header without a mount pass.
  MountPlan under = host.driver.planMount(host.offset + 1.0, inputs.windowAlong, inputs.pad);
  if (under.low == before.low && under.high == before.high) {
    CHECK(shouldSkipMount(recordOf(before, inputs), recordOf(under, inputs), host.offset + 1.0, 1, false));
  }
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

TEST(list_driver_reset_holds_a_row_whose_estimate_sits_at_the_offset) {
  // Every row measures the estimate until the reset, and two thirds of it after.
  double size = 120.0;
  Host host(200);
  host.driver.setMeasureRow([&size](std::size_t, const std::string&, double) { return size; });
  host.settle();
  host.scrollTo(3 * 120.0 + 30.0);
  host.settle();
  size = 80.0;
  host.driver.resetKeepingPosition();
  host.layout();
  CHECK(!host.settling);
  CHECK_NEAR(host.offset - host.driver.getLeadingAt(3), 30.0, 0.01);
}

TEST(list_driver_reset_measures_the_row_the_viewport_is_deep_into) {
  // Row 6 is taller than its estimate and the window together.
  Host host(200);
  host.driver.setMeasureRow([](std::size_t index, const std::string&, double) { return index == 6 ? 3000.0 : 60.0; });
  host.settle();
  host.scrollTo(host.driver.getLeadingAt(6) + 2000.0);
  host.settle();
  host.driver.resetKeepingPosition();
  host.layout();
  CHECK_EQ(host.driver.getExtentAt(6), 3000.0);
  CHECK_NEAR(host.offset - host.driver.getLeadingAt(6), 2000.0, 0.01);
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
  auto measured = host.driver.getMeasuredRange();
  CHECK(measured->low < visible->low);
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

TEST(list_driver_drag_reaches_the_last_and_first_slot_through_the_content_edge) {
  Host host(3);
  host.settle();
  std::vector<std::size_t> mounted{0, 1, 2};

  host.driver.dragBegin(0, host.driver.getLeadingAt(0) + 5.0, 0.0);
  host.driver.placeHeld(0, 100000.0, 0.0);
  host.driver.dragUpdateInsertion(mounted);
  CHECK_EQ(host.driver.getDragInsertionIndex(), std::size_t{2});
  host.driver.dragEnd();

  host.driver.dragBegin(2, host.driver.getLeadingAt(2) + 5.0, 0.0);
  host.driver.placeHeld(2, -100000.0, 0.0);
  host.driver.dragUpdateInsertion(mounted);
  CHECK_EQ(host.driver.getDragInsertionIndex(), std::size_t{0});
  host.driver.dragEnd();
}

TEST(list_driver_foreign_drag_uses_target_geometry_and_keys_then_returns_to_normal) {
  Host host(8);
  host.uniformSize = 100.0;
  host.settle();
  host.driver.dragBegin(2, 210.0, 0.0);
  host.driver.dragSetHeldAway(true);
  CHECK(host.driver.isDragHeldAway());
  CHECK_EQ(host.driver.dragShiftFor(3).leading, -100.0);
  CHECK_EQ(host.driver.getDragInsertionIndex(), UNDEFINED_INDEX);
  host.driver.dragSetHeldAway(false);
  CHECK_EQ(host.driver.getDragInsertionIndex(), std::size_t{2});

  host.driver.dragBeginForeign(80.0, 200.0);
  CHECK(host.driver.isDragForeign());
  CHECK(!host.driver.isDragging());
  CHECK_EQ(host.driver.getHeldIndex(), UNDEFINED_INDEX);
  host.driver.dragPlaceForeignCenter(120.0, 0.0);
  host.driver.dragUpdateInsertion({3, 0, 2, 1});
  CHECK_EQ(host.driver.getDragInsertionIndex(), std::size_t{1});
  CHECK_EQ(host.driver.getDragInsertionKey(), std::string{"k1"});
  CHECK_EQ(host.driver.dragShiftFor(0).leading, 0.0);
  CHECK_EQ(host.driver.dragShiftFor(1).leading, 80.0);
  host.driver.dragPlaceForeignCenter(900.0, 0.0);
  host.driver.dragUpdateInsertion({4, 5, 6, 7});
  CHECK_EQ(host.driver.getDragInsertionIndex(), std::size_t{8});
  CHECK(host.driver.getDragInsertionKey().empty());
  host.driver.dragEnd();
  CHECK(!host.driver.isDragForeign());
  CHECK_EQ(host.driver.dragShiftFor(1).leading, 0.0);
  host.driver.dragBegin(1, 110.0, 0.0);
  CHECK(host.driver.isDragging());
  CHECK(!host.driver.isDragForeign());
  CHECK(!host.driver.isDragHeldAway());
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
  // The ended drag shifts no row and has no origin or drop spot left.
  CHECK_EQ(host.driver.dragShiftFor(3).leading, 0.0);
  CHECK_EQ(host.driver.getDragOriginIndex(), UNDEFINED_INDEX);
  CHECK_EQ(host.driver.getDragInsertionIndex(), UNDEFINED_INDEX);
}

TEST(list_driver_a_drop_of_the_anchor_row_keeps_the_offset) {
  ListSettings settings;
  settings.estimatedRowSize = 100.0;
  Host host(80, settings);
  host.uniformSize = 100.0;
  host.settle();
  host.scrollTo(3000.0);
  host.driver.dragBegin(30, 3010.0, 0.0);
  host.driver.setDropHint({DropHint::Kind::Moved, "k30"});
  host.driver.dragEnd();

  std::vector<std::string> keys = keysFrom(0, 80);
  std::rotate(keys.begin() + 30, keys.begin() + 31, keys.begin() + 36);
  host.driver.reloadKeys(keys);
  host.settle();
  host.driver.setDropHint({});
  CHECK_EQ(host.driver.indexOfKey("k30"), std::size_t{35});
  CHECK_NEAR(host.offset, 3000.0, 0.5);
}

TEST(list_driver_a_rejected_drop_leaves_no_hint_for_a_later_sort) {
  for (bool layoutDuringDrop : {false, true}) {
    ListSettings settings;
    settings.estimatedRowSize = 100.0;
    Host host(80, settings);
    host.uniformSize = 100.0;
    host.settle();
    host.scrollTo(3000.0);
    host.driver.setDropHint({DropHint::Kind::Moved, "k30"});
    if (layoutDuringDrop) {
      host.settle();
    }
    host.driver.setDropHint({});

    std::vector<std::string> keys = keysFrom(0, 80);
    std::rotate(keys.begin() + 30, keys.begin() + 31, keys.begin() + 36);
    host.driver.reloadKeys(keys);
    host.settle();
    CHECK_NEAR(host.offset, 3500.0, 0.5);
  }
}

TEST(list_driver_an_unrelated_edit_uses_up_a_drop_hint_without_reseeding_it) {
  ListSettings settings;
  settings.estimatedRowSize = 100.0;
  Host host(80, settings);
  host.uniformSize = 100.0;
  host.settle();
  host.scrollTo(3000.0);
  host.driver.setDropHint({DropHint::Kind::Moved, "k30"});

  std::vector<std::string> keys = keysFrom(0, 81);
  host.driver.reloadKeys(keys);
  host.settle();
  CHECK_NEAR(host.offset, 3000.0, 0.5);
  std::rotate(keys.begin() + 30, keys.begin() + 31, keys.begin() + 36);
  host.driver.reloadKeys(keys);
  host.settle();
  host.driver.setDropHint({});
  CHECK_NEAR(host.offset, 3500.0, 0.5);
}

TEST(list_driver_a_cancelled_drag_leaves_a_later_sort_holding_the_anchor) {
  ListSettings settings;
  settings.estimatedRowSize = 100.0;
  Host host(80, settings);
  host.uniformSize = 100.0;
  host.settle();
  host.scrollTo(3000.0);
  host.driver.dragBegin(30, 3010.0, 0.0);
  host.driver.dragEnd();

  std::vector<std::string> keys = keysFrom(0, 80);
  std::rotate(keys.begin() + 30, keys.begin() + 31, keys.begin() + 36);
  host.driver.reloadKeys(keys);
  host.settle();
  CHECK_NEAR(host.offset, 3500.0, 0.5);
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

TEST(list_driver_prefetches_measured_rows_once_and_cancels_them_when_they_leave) {
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

/*
 * A key replaced inside the prefetched rows, with the ranges and the count unchanged, is a new
 * row to prefetch once the core took it.
 */
TEST(list_driver_prefetch_sees_a_key_replaced_under_the_same_ranges) {
  Host host(1000);
  host.settle();
  std::vector<std::size_t> prefetch;
  std::vector<std::size_t> cancel;
  MountPlan plan = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  host.driver.updatePrefetch(plan.low, plan.high, prefetch, cancel);
  CHECK(!prefetch.empty());
  std::size_t replaced = prefetch.back();

  // The same height as the row it replaces, which keeps every range where it was.
  host.driver.replaceKeys(replaced, 1, {"k" + std::to_string(replaced + 3000)});
  host.settle();
  MountPlan next = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  CHECK_EQ(next.low, plan.low);
  CHECK_EQ(next.high, plan.high);
  host.driver.updatePrefetch(next.low, next.high, prefetch, cancel);
  CHECK(prefetch == std::vector<std::size_t>({replaced}));
  CHECK(cancel.empty());

  host.driver.updatePrefetch(next.low, next.high, prefetch, cancel);
  CHECK(prefetch.empty());
}

/*
 * A kit without sizeForItem measures a row by making its cell. runPasses measures the rows it
 * will mount, reports the rows it prefetches before it measures them, and the call after the
 * mount finds nothing new.
 */
TEST(list_driver_reports_prefetch_before_it_measures_the_rows) {
  Host host(1000);
  std::vector<std::string> events;
  host.driver.setMeasureRow([&](std::size_t index, const std::string& key, double) {
    events.push_back("m" + std::to_string(index));
    return rowHeight(static_cast<std::size_t>(std::stoul(key.substr(1))));
  });
  std::vector<std::size_t> prefetched;
  host.driver.setPrefetchRows([&](const std::vector<std::size_t>& prefetch, const std::vector<std::size_t>&) {
    for (std::size_t index : prefetch) {
      events.push_back("p" + std::to_string(index));
      prefetched.push_back(index);
    }
  });
  auto reportedFirst = [&] {
    for (std::size_t index : prefetched) {
      auto reported = std::find(events.begin(), events.end(), "p" + std::to_string(index));
      auto measured = std::find(events.begin(), events.end(), "m" + std::to_string(index));
      CHECK(reported < measured);
    }
  };
  host.prefetch = true;
  host.settle();
  CHECK(!prefetched.empty());
  reportedFirst();
  // The rows the first layout mounts got sizes before the rest were prefetched.
  MountPlan first = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  for (std::size_t index : prefetched) {
    CHECK(index > first.high);
  }

  // A scroll shorter than the overscan brings rows into the measured range only.
  events.clear();
  prefetched.clear();
  host.scrollTo(host.offset + 300.0);
  host.settle();
  CHECK(!prefetched.empty());
  reportedFirst();
  MountPlan plan = host.driver.planMount(host.offset, WINDOW_ALONG, 0.0);
  for (std::size_t index : prefetched) {
    CHECK(index < plan.low || index > plan.high);
  }
  std::vector<std::size_t> prefetch;
  std::vector<std::size_t> cancel;
  host.driver.updatePrefetch(plan.low, plan.high, prefetch, cancel);
  CHECK(prefetch.empty() && cancel.empty());

  // Without prefetch in the input nothing is reported.
  prefetched.clear();
  host.prefetch = false;
  host.scrollTo(30000.0);
  host.settle();
  CHECK(prefetched.empty());
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

/*
 * A grid's list loses rows and changes width while off screen. Back on screen, one layout drops
 * the sizes (resetKeepingPosition) and must measure the rows at the offset, not at the top.
 */
TEST(list_driver_reset_measures_the_rows_at_the_offset_in_one_layout) {
  ListSettings settings;
  settings.estimatedRowSize = 30.0;
  Host host(5003, settings);
  host.driver.setMeasureRow([](std::size_t, const std::string&, double) { return 30.0; });
  host.settle();
  host.scrollTo(2714.0);
  host.settle();
  host.driver.reloadKeys(keysFrom(0, 3802));
  host.driver.reloadKeys(keysFrom(0, 802));
  host.driver.resetKeepingPosition();
  host.layout();
  auto measured = host.driver.getMeasuredRange();
  CHECK(measured.has_value());
  CHECK_NEAR(host.offset, 2714.0, 0.01);
  CHECK(host.driver.getLeadingAt(measured->low) <= host.offset);
  CHECK(host.driver.getLeadingAt(measured->high) + 30.0 >= host.offset + WINDOW_ALONG);
}

/*
 * The last row moves no row when it grows, and a host that copies the frames by the geometry
 * version still hears that it changed.
 */
TEST(list_driver_a_last_row_that_grows_moves_the_geometry_version) {
  ListDriver driver;
  double last = 50.0;
  driver.setMeasureRow([&](std::size_t index, const std::string&, double) { return index == 2 ? last : 50.0; });
  driver.setKeys({"a", "b", "c"});
  PassInput input;
  input.windowAlong = 600;
  input.windowCross = 300;
  driver.runPasses(input);
  std::uint64_t before = driver.getGeometryVersion();
  last = 200.0;
  driver.markRemeasure({2});
  PassResult result = driver.runPasses(input);
  CHECK_EQ(driver.getExtentAt(2), 200.0);
  CHECK_EQ(result.contentAlong, 300.0);
  CHECK(driver.getGeometryVersion() != before);
  // Measured again at the same size, nothing changed.
  std::uint64_t grown = driver.getGeometryVersion();
  driver.markRemeasure({2});
  driver.runPasses(input);
  CHECK_EQ(driver.getGeometryVersion(), grown);
}

/*
 * The core leaves an inverted list where it was when its content shrinks. The driver brings a
 * resting one back to the new end and runs there in the same layout.
 */
TEST(list_driver_brings_a_resting_list_back_from_past_the_end) {
  ListSettings settings;
  settings.inverted = true;
  Host host(400, settings);
  host.settle();
  double end = host.content - WINDOW_ALONG;
  CHECK_NEAR(host.offset, end, 0.5);
  host.reachedEnd = false;
  host.driver.reloadKeys(keysFrom(0, 3));
  Host unclamped(400, settings);
  unclamped.settle();
  unclamped.driver.reloadKeys(keysFrom(0, 3));
  unclamped.layout();
  CHECK(unclamped.offset > std::max(0.0, unclamped.content - WINDOW_ALONG) + 1.0);

  host.clampEnd = true;
  PassResult result = host.layout();
  CHECK_EQ(result.offset, std::max(0.0, result.contentAlong - WINDOW_ALONG));
  CHECK(!result.settling);
  CHECK(host.reachedEnd);
  // The core ran at the end: its rows are measured there and one more layout changes nothing.
  CHECK(result.band.contains(result.offset));
  PassResult again = host.layout();
  CHECK_EQ(again.offset, result.offset);
  CHECK(!again.reachedEnd);
}

TEST(list_driver_leaves_a_list_a_finger_or_the_host_holds_outside_its_content) {
  Host host(100);
  host.uniformSize = 50.0;
  host.settle();
  double end = host.content - WINDOW_ALONG;
  // No end is the driver's to hold.
  host.offset = end + 80.0;
  CHECK_EQ(host.layout().offset, end + 80.0);
  host.offset = -80.0;
  CHECK_EQ(host.layout().offset, -80.0);
  // A finger holds the list past either end.
  host.clampStart = true;
  host.clampEnd = true;
  host.tracking = true;
  CHECK_EQ(host.layout().offset, -80.0);
  host.offset = end + 80.0;
  CHECK_EQ(host.layout().offset, end + 80.0);
  // Each end comes back on its own.
  host.tracking = false;
  host.clampEnd = false;
  CHECK_EQ(host.layout().offset, end + 80.0);
  host.clampEnd = true;
  CHECK_EQ(host.layout().offset, end);
  host.offset = -80.0;
  host.clampStart = false;
  CHECK_EQ(host.layout().offset, -80.0);
  host.clampStart = true;
  CHECK_EQ(host.layout().offset, 0.0);
  CHECK_EQ(host.content - WINDOW_ALONG, end);
}

TEST(list_driver_lets_a_list_rest_within_the_slack_of_an_end) {
  Host host(100);
  host.uniformSize = 50.0;
  host.settle();
  host.clampStart = true;
  host.clampEnd = true;
  host.clampSlack = 0.5;
  double end = host.content - WINDOW_ALONG;
  host.offset = end + 0.4;
  CHECK_EQ(host.layout().offset, end + 0.4);
  host.offset = end + 0.6;
  CHECK_EQ(host.layout().offset, end);
  host.offset = -0.4;
  CHECK_EQ(host.layout().offset, -0.4);
  host.offset = -0.6;
  CHECK_EQ(host.layout().offset, 0.0);
  // Content shorter than the window rests at the start.
  Host small(3);
  small.clampEnd = true;
  small.offset = 300.0;
  CHECK_EQ(small.layout().offset, 0.0);
}

/*
 * An inverted list resting at its end loses every row it measured. The rows left are placed by
 * estimate and measure shorter than the window at their end. The list is drawn inside its
 * content, and a frame the driver asks for finds it at rest.
 */
TEST(list_driver_draws_a_list_brought_back_inside_the_content_its_end_run_measured) {
  for (bool clampEnd : {true, false}) {
    ListDriver driver;
    ListSettings settings;
    settings.inverted = true;
    driver.setSettings(settings);
    driver.setMeasureRow([](std::size_t, const std::string& key, double) {
      return std::stoul(key.substr(1)) < 3 ? 150.0 : 220.0;
    });
    driver.setKeys(keysFrom(0, 20));
    PassInput input;
    input.windowAlong = WINDOW_ALONG;
    input.windowCross = WINDOW_CROSS;
    input.clampEnd = clampEnd;
    PassResult result = driver.runPasses(input);
    input.offset = result.offset;
    result = driver.runPasses(input);
    CHECK_EQ(result.offset, result.contentAlong - WINDOW_ALONG);
    CHECK(!result.settling);

    std::vector<std::size_t> deleted;
    for (std::size_t index = 3; index < 20; ++index) {
      deleted.push_back(index);
    }
    driver.deleteKeys(deleted);
    result = driver.runPasses(input);
    if (!clampEnd) {
      // Without the driver holding the end the list stays outside its content.
      CHECK(result.offset > 0.0);
      continue;
    }
    CHECK_EQ(result.contentAlong, 450.0);
    CHECK_EQ(result.offset, 0.0);
    for (int frame = 0; frame < 3 && result.settling; ++frame) {
      input.offset = result.offset;
      result = driver.runPasses(input);
    }
    CHECK_EQ(result.offset, 0.0);
    CHECK(!result.settling);
  }
}

TEST(list_driver_reports_an_inverted_list_leaving_its_end_and_coming_back) {
  ListSettings settings;
  settings.inverted = true;
  Host host(60, settings);
  host.settle();
  double end = host.content - WINDOW_ALONG;
  CHECK(host.driver.isAtEnd());
  CHECK(host.layout().atEnd);
  // The reader scrolls away from the newest row.
  host.scrollTo(end - 400.0);
  CHECK(!host.layout().atEnd);
  CHECK(!host.driver.isAtEnd());
  // Rows appended while away leave the list away from its end.
  std::vector<std::string> keys = keysFrom(0, 70);
  host.driver.reloadKeys(keys);
  host.settle();
  CHECK(!host.layout().atEnd);
  // A scroll command that lands on the end brings it back.
  host.driver.scrollToEnd();
  host.settle();
  CHECK(host.layout().atEnd);
  // One that lands away from the end leaves it again.
  host.driver.scrollToRow(5, 0.0);
  host.settle();
  CHECK(!host.layout().atEnd);
}

TEST(list_driver_a_list_that_is_not_inverted_stays_at_its_end) {
  Host host(200);
  host.layout();
  host.scrollTo(900.0);
  CHECK(host.layout().atEnd);
  host.driver.scrollToRow(100, 0.0);
  host.settle();
  CHECK(host.layout().atEnd);
}

TEST(list_driver_reset_leaves_an_inverted_list_away_from_its_end) {
  ListSettings settings;
  settings.inverted = true;
  Host host(200, settings);
  host.settle();
  host.scrollTo(host.content - WINDOW_ALONG - 900.0);
  CHECK(!host.layout().atEnd);
  host.driver.resetKeepingPosition();
  // The layout that lands the held row already says where the list is.
  CHECK(!host.layout().atEnd);
  host.settle();
  CHECK(!host.layout().atEnd);
}

TEST(list_driver_never_holds_the_content_by_a_non_anchor_key) {
  // A row at the viewport start that grows keeps its place, unless another row holds the content.
  enum class Case { Anchor, NonAnchor, NonAnchorAfterReset };
  for (Case which : {Case::Anchor, Case::NonAnchor, Case::NonAnchorAfterReset}) {
    Host host(100);
    double size = 60.0;
    host.driver.setMeasureRow([&size](std::size_t, const std::string& key, double) {
      return key == "k10" ? size : 60.0;
    });
    if (which != Case::Anchor) {
      host.driver.setNonAnchorKeys({"k10"});
    }
    host.layout();
    if (which == Case::NonAnchorAfterReset) {
      // A new core takes the keys again.
      host.driver.resetKeepingPosition();
      host.layout();
    }
    host.scrollTo(600.0);
    host.layout();
    CHECK_EQ(host.driver.getLeadingAt(10), 600.0);
    size = 100.0;
    host.driver.markRemeasure({10});
    host.driver.reloadKeys(keysFrom(0, 101));
    host.settle();
    if (which == Case::Anchor) {
      CHECK_NEAR(host.driver.getLeadingAt(10) - host.offset, 0.0, 0.01);
    } else {
      // The row after it holds the content: it stays 60 below the viewport start.
      CHECK_NEAR(host.driver.getLeadingAt(11) - host.offset, 60.0, 0.01);
    }
  }
}

TEST(list_driver_counts_the_empty_view_with_the_header_while_it_has_no_rows) {
  Host host(0);
  PassInput input;
  input.windowAlong = WINDOW_ALONG;
  input.windowCross = WINDOW_CROSS;
  input.headerSize = 50.0;
  input.footerSize = 40.0;
  input.emptySize = 300.0;
  PassResult result = host.driver.runPasses(input);
  CHECK(result.showsEmpty);
  CHECK(host.driver.showsEmpty());
  // The footer goes after the empty view and the content holds all three.
  CHECK_EQ(result.footerStart, 350.0);
  CHECK_EQ(result.contentAlong, 390.0);

  host.driver.reloadKeys(keysFrom(0, 3));
  result = host.driver.runPasses(input);
  CHECK(!result.showsEmpty);
  // Three rows of 50, 60 and 70 follow the header, without the empty view.
  CHECK_EQ(host.driver.getLeadingAt(0), 50.0);
  CHECK_EQ(result.footerStart, 230.0);
  CHECK_EQ(result.contentAlong, 270.0);

  host.driver.deleteKeys({0, 1, 2});
  result = host.driver.runPasses(input);
  CHECK(result.showsEmpty);
  CHECK_EQ(result.footerStart, 350.0);
  CHECK_EQ(result.contentAlong, 390.0);
}

/*
 * A prepend to short content can settle on an end before its old row fits. A later content
 * edit starts a new hold at the row now on screen, rather than restoring that old row.
 */
TEST(list_driver_remeasuring_changed_content_releases_a_previous_clamped_hold) {
  Host host(1);
  double lastSize = 200.0;
  host.driver.setMeasureRow([&lastSize](std::size_t, const std::string& key, double) {
    return key == "k0" ? lastSize : 200.0;
  });
  host.settle();
  host.driver.setKeys({"k1", "k2", "k3", "k4", "k5", "k6", "k0"});
  host.settle();
  CHECK_NEAR(host.offset, 800.0, 0.5);
  lastSize = 218.0;
  host.driver.markRemeasure({6});
  host.settle();
  CHECK_NEAR(host.offset, 800.0, 0.5);
}

TEST(list_driver_auto_hide_keeps_user_intent_across_corrections_and_flag_changes) {
  ListDriver driver;
  StickyInput input;
  input.windowSize = 400.0;
  input.contentSize = 2000.0;
  input.hasHeader = input.hasFooter = true;
  input.headerSize = 60.0;
  input.footerSize = 40.0;
  input.footerStart = 1960.0;
  input.autoHideHeader = input.autoHideFooter = true;
  input.offset = 200.0;
  driver.stickyTranslations(input); // A command rebases without hiding.
  input.accumulate = true;
  input.offset = 225.0;
  auto moved = driver.stickyTranslations(input);
  CHECK_EQ(moved.header, 200.0);
  CHECK_EQ(moved.footer, 225.0 + 400.0 - 40.0 - 1960.0 + 25.0);
  input.accumulate = false;
  input.offset = 325.0; // A prepend correction is separate from the finger's 25.
  auto corrected = driver.stickyTranslations(input);
  CHECK_EQ(corrected.header, 300.0);
  input.autoHideHeader = false;
  driver.stickyTranslations(input);
  input.autoHideHeader = true;
  CHECK_EQ(driver.stickyTranslations(input).header, 300.0);
  input.offset = 315.0;
  input.accumulate = true;
  CHECK_EQ(driver.stickyTranslations(input).header, 300.0);
  // Repeated layout of the same input cannot count the movement twice.
  CHECK_EQ(driver.stickyTranslations(input).header, 300.0);
  ListDriver other;
  input.accumulate = false;
  CHECK_EQ(other.stickyTranslations(input).header, 315.0);
}
