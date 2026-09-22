/*
 * Keeping the visible content in place, and scroll commands, while the layout changes
 * underneath: rows above the screen resize, the list shrinks, and commands arrive with data.
 * The row the reader is looking at, or the row a command targets, must end up in the right place.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;
using namespace slt;

namespace {

constexpr double HEADER = 189.0;
constexpr double ROW = 82.3;

FrameInput report(const std::vector<std::string>& keys, double offset, double header = HEADER) {
  FrameInput input = inputFor(keys, offset);
  input.headerSize = header;
  return input;
}

/*
 * The commit that applies the offset the layout pass asked for.
 */
FrameInput ownWrite(const Container& container, const std::vector<std::string>& keys, double header = HEADER) {
  FrameInput input = report(keys, container.revision.containerOffsetY, header);
  input.containerOffsetEnabled = true;
  input.commitToken = container.operation ? container.operation->id : 0;
  return input;
}

/*
 * The host reporting back that it applied that offset.
 */
FrameInput echo(const Container& container, const std::vector<std::string>& keys, std::uint64_t token, double header = HEADER) {
  FrameInput input = report(keys, container.revision.containerOffsetY, header);
  input.commitToken = token;
  return input;
}

/*
 * Acts like the Fabric layout pass: apply the header, give every mounted row its real size,
 * reflow once, and refresh the total, which fixes the average from the first sizes.
 */
void layoutPass(
  Container& container,
  double header,
  std::size_t mountedFrom = UNDEFINED_INDEX,
  std::size_t mountedTo = UNDEFINED_INDEX,
  double rowSize = ROW) {
  double previousHeader = container.headerSize;
  double previousWindow = container.revision.windowContainerHeight;
  if (previousHeader != header || previousWindow != WINDOW_HEIGHT) {
    container.headerSize = header;
    container.revision.windowContainerWidth = WINDOW_WIDTH;
    container.revision.windowContainerHeight = WINDOW_HEIGHT;
    Virtualizer::recomputeElementOffsets(&container, 0);
    Virtualizer::applyHeaderSizeChange(&container, previousHeader);
    Virtualizer::applyWindowSizeChange(&container, previousWindow);
    container.containerOffsetCorrected = true;
  }
  auto visible = container.getVisibleIndices();
  if (mountedFrom == UNDEFINED_INDEX && visible.first != UNDEFINED_INDEX) {
    mountedFrom = std::min(visible.first, visible.second);
    mountedTo = std::max(visible.first, visible.second);
  }
  if (mountedFrom != UNDEFINED_INDEX) {
    std::size_t from = mountedFrom;
    std::size_t to = std::min(mountedTo, container.revision.elements.size() - 1);
    std::size_t lowest = UNDEFINED_INDEX;
    for (std::size_t index = from; index <= to; ++index) {
      if (Virtualizer::applyElementSize(&container, index, {WINDOW_WIDTH, rowSize}) && index < lowest) {
        lowest = index;
      }
    }
    if (lowest != UNDEFINED_INDEX) {
      Virtualizer::commitElementSizes(&container, lowest);
    }
  }
  Virtualizer::recomputeTotalSize(&container);
}

double onScreen(const Container& container, const std::string& key) {
  return offsetOf(container, container.findElementIndexByKey(key)) - container.revision.containerOffsetY;
}

}

/*
 * A list opens deep enough that the rows above are still estimated when the average is fixed.
 * They shrink to the average and so does the total, so the first offset is now past the end.
 * The target must still land at the top.
 */
TEST(scroll_to_index_holds_its_row_when_the_rows_above_shrink_to_the_average) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  container.scrollToIndex(30);
  // The first commit runs before layout, so there is no window or header size yet.
  FrameInput first = report(keys, 0.0, 0.0);
  first.windowContainerWidth = 0.0;
  first.windowContainerHeight = 0.0;
  Virtualizer::update(&container, first);
  // The first layout mounts the rows around the target, a few of them above it.
  layoutPass(container, HEADER, 26, 49);
  std::uint64_t token = container.operation ? container.operation->id : 0;
  CHECK(token != 0);
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);

  Virtualizer::update(&container, ownWrite(container, keys));
  layoutPass(container, HEADER, 26, 49);
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);

  for (int commit = 0; commit < 4; ++commit) {
    std::uint64_t current = container.operation ? container.operation->id : token;
    Virtualizer::update(&container, echo(container, keys, current));
    layoutPass(container, HEADER, 26, 49);
  }
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);
  CHECK(!container.containerOffsetCorrected);
}

/*
 * The same shrink with no command running. The reader rests near the end while the rows above
 * are still estimated. The row they are reading stays put instead of jumping to the new end.
 */
TEST(rows_above_shrinking_near_the_end_keep_the_visible_row) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  Virtualizer::update(&container, report(keys, 0.0));
  // Jump deep without measuring anything above, so rows 0 to 29 stay estimated.
  double deep = HEADER + 30 * ESTIMATED_ROW_HEIGHT;
  Virtualizer::update(&container, report(keys, deep));
  Virtualizer::update(&container, report(keys, deep));
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);

  // Rows from 30 on get real sizes, which fixes a smaller average.
  for (std::size_t index = 30; index < 50; ++index) {
    Virtualizer::applyElementSize(&container, index, {WINDOW_WIDTH, ROW});
  }
  Virtualizer::commitElementSizes(&container, 30);
  Virtualizer::recomputeTotalSize(&container);
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);

  for (int commit = 0; commit < 4; ++commit) {
    std::uint64_t current = container.operation ? container.operation->id : 0;
    if (container.containerOffsetCorrected) {
      Virtualizer::update(&container, ownWrite(container, keys));
    }
    Virtualizer::update(&container, echo(container, keys, current));
  }
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);
  CHECK(!container.containerOffsetCorrected);
}

/*
 * A refresh prepends new rows and drops the row at the top of the screen, as if the server
 * deleted that post. The next row on screen holds its place instead of the new rows showing.
 */
TEST(refresh_that_drops_the_anchor_row_holds_the_next_visible_row) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, 12, 100.0);
  Virtualizer::update(&container, report(keys, 0.0));
  double k1Before = onScreen(container, "k1");

  std::vector<std::string> refreshed = keysFor(6, "new");
  refreshed.insert(refreshed.end(), keys.begin() + 1, keys.end());
  Virtualizer::update(&container, report(refreshed, 0.0));
  CHECK_NEAR(onScreen(container, "k1"), k1Before, 0.5);
  std::uint64_t token = container.operation ? container.operation->id : 0;
  CHECK(token != 0);
  layoutPass(container, HEADER, 0, 20, 100.0);
  Virtualizer::update(&container, ownWrite(container, refreshed));
  layoutPass(container, HEADER, 0, 20, 100.0);
  Virtualizer::update(&container, echo(container, refreshed, token));
  Virtualizer::update(&container, echo(container, refreshed, token));
  CHECK_NEAR(onScreen(container, "k1"), k1Before, 0.5);
  CHECK(!container.containerOffsetCorrected);
}

/*
 * The same in the middle of the list: the row cut by the top of the screen is removed while
 * rows are inserted above it. The next visible row keeps its place.
 */
TEST(insert_above_that_removes_the_straddling_row_holds_the_next_visible_row) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(&container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, 59, 100.0);
  double offset = HEADER + 20 * 100.0 + 40.0;
  Virtualizer::update(&container, report(keys, offset));
  Virtualizer::update(&container, report(keys, offset));
  double k21Before = onScreen(container, "k21");
  CHECK_NEAR(k21Before, 60.0, 0.5);

  std::vector<std::string> edited = keysFor(4, "ins");
  edited.insert(edited.end(), keys.begin(), keys.begin() + 20);
  edited.insert(edited.end(), keys.begin() + 21, keys.end());
  Virtualizer::update(&container, report(edited, offset));
  CHECK_NEAR(onScreen(container, "k21"), k21Before, 0.5);
}

namespace {

struct Variant {
  const char* name;
  bool horizontal;
  bool inverted;
  std::size_t columns;
};

const Variant VARIANTS[] = {
  {"vertical", false, false, 1},
  {"horizontal", true, false, 1},
  {"inverted", false, true, 1},
  {"grid", false, false, 3},
};

FrameInput variantFrame(const Variant& variant, const std::vector<std::string>& keys, double offset) {
  FrameInput input = inputFor(keys, 0.0);
  input.horizontal = variant.horizontal;
  input.inverted = variant.inverted;
  input.columns = variant.columns;
  if (variant.horizontal) {
    input.containerOffsetX = offset;
    input.estimatedElementSize = {ESTIMATED_ROW_HEIGHT, WINDOW_HEIGHT};
  } else {
    input.containerOffsetY = offset;
  }
  return input;
}

double scrollOffset(const Container& container) {
  return container.horizontal ? container.revision.containerOffsetX : container.revision.containerOffsetY;
}

double screenPos(const Container& container, const std::string& key) {
  return offsetOf(container, container.findElementIndexByKey(key)) - scrollOffset(container);
}

Size sized(const Container& container, double mainAxis) {
  return container.horizontal ? Size{mainAxis, WINDOW_HEIGHT} : Size{WINDOW_WIDTH, mainAxis};
}

/*
 * Scroll to the middle and settle with every row 100 tall and row k30 exactly at the top.
 * In a grid k30 is in the first column.
 */
void settleMidList(Container& container, const Variant& variant, const std::vector<std::string>& keys) {
  Virtualizer::update(&container, variantFrame(variant, keys, 0.0));
  // An inverted list opens at its bottom, so drag away from it first.
  FrameInput away = variantFrame(variant, keys, 100.0);
  away.userScrolled = true;
  away.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, away);
  for (std::size_t index = 0; index < keys.size(); ++index) {
    Virtualizer::updateElementAtIndex(&container, index, sized(container, 100.0));
  }
  double rest = offsetOf(container, container.findElementIndexByKey("k30"));
  FrameInput drag = variantFrame(variant, keys, rest);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, drag);
  Virtualizer::update(&container, variantFrame(variant, keys, rest));
  Virtualizer::update(&container, variantFrame(variant, keys, rest));
}

/*
 * Run the commits that follow a change: apply the core's offset, then the host's report back.
 */
void settleCommits(Container& container, const Variant& variant, const std::vector<std::string>& keys) {
  for (int round = 0; round < 3; ++round) {
    std::uint64_t token = container.operation ? container.operation->id : 0;
    if (container.containerOffsetCorrected) {
      FrameInput write = variantFrame(variant, keys, scrollOffset(container));
      write.containerOffsetEnabled = true;
      write.commitToken = token;
      Virtualizer::update(&container, write);
    }
    FrameInput echoed = variantFrame(variant, keys, scrollOffset(container));
    echoed.commitToken = token;
    Virtualizer::update(&container, echoed);
  }
}

}

/*
 * A mounted row above the screen resizes, like an edit or an image loading. In every layout
 * the rows on screen stay put, both in that layout pass and in every commit after.
 */
TEST(resizing_a_row_above_the_viewport_keeps_the_visible_rows_in_every_layout) {
  for (const Variant& variant : VARIANTS) {
    std::vector<std::string> keys = keysFor(90);
    Container container;
    settleMidList(container, variant, keys);
    double before = screenPos(container, "k30");
    double nextBefore = screenPos(container, "k33");

    /*
     * Each grid column stacks on its own, so one offset cannot hold two columns when only one
     * resizes. In a grid the whole row above resizes instead.
     */
    std::vector<std::string> resized = variant.columns > 1
      ? std::vector<std::string>{"k27", "k28", "k29"}
      : std::vector<std::string>{"k27"};
    for (double size : {260.0, 40.0, 100.0}) {
      std::size_t lowest = UNDEFINED_INDEX;
      for (const std::string& key : resized) {
        std::size_t index = container.findElementIndexByKey(key);
        if (Virtualizer::applyElementSize(&container, index, sized(container, size)) && index < lowest) {
          lowest = index;
        }
      }
      Virtualizer::commitElementSizes(&container, lowest);
      Virtualizer::recomputeTotalSize(&container);
      if (std::fabs(screenPos(container, "k30") - before) > 0.5) {
        fail(std::string(variant.name) + ": k30 moved on the layout that resized k27 to " + std::to_string(size));
      }
      settleCommits(container, variant, keys);
      if (std::fabs(screenPos(container, "k30") - before) > 0.5 ||
          std::fabs(screenPos(container, "k33") - nextBefore) > 0.5) {
        fail(std::string(variant.name) + ": visible rows moved after k27 resized to " + std::to_string(size));
      }
    }
  }
}

/*
 * Rows removed and inserted above the screen, then a prepend. In every layout the rows on
 * screen stay where they are.
 */
TEST(inserting_and_removing_rows_above_the_viewport_keeps_the_visible_rows_in_every_layout) {
  for (const Variant& variant : VARIANTS) {
    std::vector<std::string> keys = keysFor(90);
    Container container;
    settleMidList(container, variant, keys);
    double before = screenPos(container, "k30");

    // Remove three rows above, a whole row in a grid so the columns stay aligned.
    std::vector<std::string> removed = keys;
    removed.erase(removed.begin() + 12, removed.begin() + 15);
    Virtualizer::update(&container, variantFrame(variant, removed, scrollOffset(container)));
    settleCommits(container, variant, removed);
    if (std::fabs(screenPos(container, "k30") - before) > 0.5) {
      fail(std::string(variant.name) + ": k30 moved when rows above were removed");
    }

    // Insert three rows above, in the middle of the list.
    std::vector<std::string> inserted = removed;
    std::vector<std::string> fresh = keysFor(3, "mid");
    inserted.insert(inserted.begin() + 5, fresh.begin(), fresh.end());
    Virtualizer::update(&container, variantFrame(variant, inserted, scrollOffset(container)));
    settleCommits(container, variant, inserted);
    if (std::fabs(screenPos(container, "k30") - before) > 0.5) {
      fail(std::string(variant.name) + ": k30 moved when rows were inserted above");
    }

    // Add six rows at the start.
    std::vector<std::string> prepended = keysFor(6, "pre");
    prepended.insert(prepended.end(), inserted.begin(), inserted.end());
    Virtualizer::update(&container, variantFrame(variant, prepended, scrollOffset(container)));
    settleCommits(container, variant, prepended);
    if (std::fabs(screenPos(container, "k30") - before) > 0.5) {
      fail(std::string(variant.name) + ": k30 moved when rows were prepended");
    }
  }
}
