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
#include <cstddef>
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
  FrameInput input = report(keys, container.revision.offsetY, header);
  input.offsetEnabled = true;
  input.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
  return input;
}

/*
 * The host reporting back that it applied that offset.
 */
FrameInput echo(
  const Container& container,
  const std::vector<std::string>& keys,
  std::uint64_t token,
  double header = HEADER) {
  FrameInput input = report(keys, container.revision.offsetY, header);
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
  double previousWindow = container.revision.windowHeight;
  if (previousHeader != header || previousWindow != WINDOW_HEIGHT) {
    container.headerSize = header;
    container.revision.windowWidth = WINDOW_WIDTH;
    container.revision.windowHeight = WINDOW_HEIGHT;
    Virtualizer::recomputeRowOffsets(container, 0);
    Virtualizer::applyHeaderSizeChange(container, previousHeader);
    Virtualizer::applyWindowSizeChange(container, previousWindow);
    container.state.offsetCorrected = true;
  }
  auto visible = container.getMeasuredRange();
  if (mountedFrom == UNDEFINED_INDEX && visible.low != UNDEFINED_INDEX) {
    mountedFrom = visible.low;
    mountedTo = visible.high;
  }
  if (mountedFrom != UNDEFINED_INDEX) {
    std::size_t from = mountedFrom;
    std::size_t to = std::min(mountedTo, container.revision.rows.size() - 1);
    std::size_t lowest = UNDEFINED_INDEX;
    for (std::size_t index = from; index <= to; ++index) {
      if (Virtualizer::applyRowSize(container, index, {WINDOW_WIDTH, rowSize}) && index < lowest) {
        lowest = index;
      }
    }
    if (lowest != UNDEFINED_INDEX) {
      Virtualizer::commitRowSizes(container, lowest);
    }
  }
  Virtualizer::recomputeContentSize(container);
}

double onScreen(const Container& container, const std::string& key) {
  return offsetOf(container, container.indexOfKey(key)) - container.revision.offsetY;
}

}

/*
 * A list opens deep enough that the rows above are still estimated when the average is fixed.
 * They shrink to the average and so does the total. The first offset is now past the end.
 * The target must still land at the top.
 */
TEST(scroll_to_index_holds_its_row_when_the_rows_above_shrink_to_the_average) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  container.scrollToRow(30);
  // The first commit runs before layout. There is no window or header size yet.
  FrameInput first = report(keys, 0.0, 0.0);
  first.windowWidth = 0.0;
  first.windowHeight = 0.0;
  Virtualizer::update(container, first);
  // The first layout mounts the rows around the target, a few of them above it.
  layoutPass(container, HEADER, 26, 49);
  std::uint64_t token = container.state.operation ? container.state.operation->commitToken : 0;
  CHECK(token != 0);
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);

  Virtualizer::update(container, ownWrite(container, keys));
  layoutPass(container, HEADER, 26, 49);
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);

  for (int commit = 0; commit < 4; ++commit) {
    std::uint64_t current = container.state.operation ? container.state.operation->commitToken : token;
    Virtualizer::update(container, echo(container, keys, current));
    layoutPass(container, HEADER, 26, 49);
  }
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);
  CHECK(!container.state.offsetCorrected);
}

/*
 * The same shrink with no command running. The reader rests near the end while the rows above
 * are still estimated. The row they are reading stays put instead of jumping to the new end.
 */
TEST(rows_above_shrinking_near_the_end_keep_the_visible_row) {
  std::vector<std::string> keys = keysFor(50);
  Container container;
  Virtualizer::update(container, report(keys, 0.0));
  // Jump deep without measuring anything above. Rows 0 to 29 stay estimated.
  double deep = HEADER + 30 * ESTIMATED_ROW_HEIGHT;
  Virtualizer::update(container, report(keys, deep));
  Virtualizer::update(container, report(keys, deep));
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);

  // Rows from 30 on get real sizes, which fixes a smaller average.
  for (std::size_t index = 30; index < 50; ++index) {
    Virtualizer::applyRowSize(container, index, {WINDOW_WIDTH, ROW});
  }
  Virtualizer::commitRowSizes(container, 30);
  Virtualizer::recomputeContentSize(container);
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);

  for (int commit = 0; commit < 4; ++commit) {
    std::uint64_t current = container.state.operation ? container.state.operation->commitToken : 0;
    if (container.state.offsetCorrected) {
      Virtualizer::update(container, ownWrite(container, keys));
    }
    Virtualizer::update(container, echo(container, keys, current));
  }
  CHECK_NEAR(onScreen(container, "k30"), 0.0, 0.5);
  CHECK(!container.state.offsetCorrected);
}

/*
 * A refresh prepends new rows and drops the row at the top of the screen, as if the server
 * deleted that post. The next row on screen holds its place instead of the new rows showing.
 */
TEST(refresh_that_drops_the_anchor_row_holds_the_next_visible_row) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, 12, 100.0);
  Virtualizer::update(container, report(keys, 0.0));
  double k1Before = onScreen(container, "k1");

  std::vector<std::string> refreshed = keysFor(6, "new");
  refreshed.insert(refreshed.end(), keys.begin() + 1, keys.end());
  Virtualizer::update(container, report(refreshed, 0.0));
  CHECK_NEAR(onScreen(container, "k1"), k1Before, 0.5);
  std::uint64_t token = container.state.operation ? container.state.operation->commitToken : 0;
  CHECK(token != 0);
  layoutPass(container, HEADER, 0, 20, 100.0);
  Virtualizer::update(container, ownWrite(container, refreshed));
  layoutPass(container, HEADER, 0, 20, 100.0);
  Virtualizer::update(container, echo(container, refreshed, token));
  Virtualizer::update(container, echo(container, refreshed, token));
  CHECK_NEAR(onScreen(container, "k1"), k1Before, 0.5);
  CHECK(!container.state.offsetCorrected);
}

/*
 * The same in the middle of the list: the row cut by the top of the screen is removed while
 * rows are inserted above it. The next visible row keeps its place.
 */
TEST(insert_above_that_removes_the_straddling_row_holds_the_next_visible_row) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, 59, 100.0);
  double offset = HEADER + 20 * 100.0 + 40.0;
  Virtualizer::update(container, report(keys, offset));
  Virtualizer::update(container, report(keys, offset));
  double k21Before = onScreen(container, "k21");
  CHECK_NEAR(k21Before, 60.0, 0.5);

  std::vector<std::string> edited = keysFor(4, "ins");
  edited.insert(edited.end(), keys.begin(), keys.begin() + 20);
  edited.insert(edited.end(), keys.begin() + 21, keys.end());
  Virtualizer::update(container, report(edited, offset));
  CHECK_NEAR(onScreen(container, "k21"), k21Before, 0.5);
}

namespace {

struct Variant {
  const char* name;
  bool horizontal;
  bool inverted;
  std::size_t numberOfColumns;
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
  input.numberOfColumns = variant.numberOfColumns;
  if (variant.horizontal) {
    input.offsetX = offset;
    input.estimatedRowSize = {ESTIMATED_ROW_HEIGHT, WINDOW_HEIGHT};
  } else {
    input.offsetY = offset;
  }
  return input;
}

double scrollOffset(const Container& container) {
  return container.horizontal ? container.revision.offsetX : container.revision.offsetY;
}

double screenPos(const Container& container, const std::string& key) {
  return offsetOf(container, container.indexOfKey(key)) - scrollOffset(container);
}

Size sized(const Container& container, double mainAxis) {
  return container.horizontal ? Size{mainAxis, WINDOW_HEIGHT} : Size{WINDOW_WIDTH, mainAxis};
}

/*
 * Scroll to the middle and settle with every row 100 tall and row k30 exactly at the top.
 * In a grid k30 is in the first column.
 */
void settleMidList(Container& container, const Variant& variant, const std::vector<std::string>& keys) {
  Virtualizer::update(container, variantFrame(variant, keys, 0.0));
  // An inverted list opens at its bottom. Drag away from it first.
  FrameInput away = variantFrame(variant, keys, 100.0);
  away.userScrolled = true;
  away.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, away);
  for (std::size_t index = 0; index < keys.size(); ++index) {
    Virtualizer::updateRowAtIndex(container, index, sized(container, 100.0));
  }
  double rest = offsetOf(container, container.indexOfKey("k30"));
  FrameInput drag = variantFrame(variant, keys, rest);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, drag);
  Virtualizer::update(container, variantFrame(variant, keys, rest));
  Virtualizer::update(container, variantFrame(variant, keys, rest));
}

/*
 * Run the commits that follow a change: apply the core's offset, then the host's report back.
 */
void settleCommits(Container& container, const Variant& variant, const std::vector<std::string>& keys) {
  for (int round = 0; round < 3; ++round) {
    std::uint64_t token = container.state.operation ? container.state.operation->commitToken : 0;
    if (container.state.offsetCorrected) {
      FrameInput write = variantFrame(variant, keys, scrollOffset(container));
      write.offsetEnabled = true;
      write.commitToken = token;
      Virtualizer::update(container, write);
    }
    FrameInput echoed = variantFrame(variant, keys, scrollOffset(container));
    echoed.commitToken = token;
    Virtualizer::update(container, echoed);
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
     * Each grid column stacks on its own. One offset cannot hold two columns when only one
     * resizes. In a grid the whole row above resizes instead.
     */
    std::vector<std::string> resized = variant.numberOfColumns > 1
      ? std::vector<std::string>{"k27", "k28", "k29"}
      : std::vector<std::string>{"k27"};
    for (double size : {260.0, 40.0, 100.0}) {
      std::size_t lowest = UNDEFINED_INDEX;
      for (const std::string& key : resized) {
        std::size_t index = container.indexOfKey(key);
        if (Virtualizer::applyRowSize(container, index, sized(container, size)) && index < lowest) {
          lowest = index;
        }
      }
      Virtualizer::commitRowSizes(container, lowest);
      Virtualizer::recomputeContentSize(container);
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
    Virtualizer::update(container, variantFrame(variant, removed, scrollOffset(container)));
    settleCommits(container, variant, removed);
    if (std::fabs(screenPos(container, "k30") - before) > 0.5) {
      fail(std::string(variant.name) + ": k30 moved when rows above were removed");
    }

    // Insert three rows above, in the middle of the list.
    std::vector<std::string> inserted = removed;
    std::vector<std::string> fresh = keysFor(3, "mid");
    inserted.insert(inserted.begin() + 5, fresh.begin(), fresh.end());
    Virtualizer::update(container, variantFrame(variant, inserted, scrollOffset(container)));
    settleCommits(container, variant, inserted);
    if (std::fabs(screenPos(container, "k30") - before) > 0.5) {
      fail(std::string(variant.name) + ": k30 moved when rows were inserted above");
    }

    // Add six rows at the start.
    std::vector<std::string> prepended = keysFor(6, "pre");
    prepended.insert(prepended.end(), inserted.begin(), inserted.end());
    Virtualizer::update(container, variantFrame(variant, prepended, scrollOffset(container)));
    settleCommits(container, variant, prepended);
    if (std::fabs(screenPos(container, "k30") - before) > 0.5) {
      fail(std::string(variant.name) + ": k30 moved when rows were prepended");
    }
  }
}

/*
 * A scroll command reaches the core through state, and an older state can come through after
 * it with the sequence from before the command. That must not run the command again, or a
 * reader who scrolled away after the jump is pulled back to the target.
 */
TEST(an_older_command_sequence_does_not_run_the_scroll_command_again) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, 59, 100.0);

  container.requestScrollToRow(30, 1.0, UNDEFINED_INDEX);
  Virtualizer::update(container, report(keys, 0.0));
  double target = container.revision.offsetY;
  CHECK(container.state.operation && container.state.operation->type == OperationType::ScrollToKey);
  std::uint64_t token = container.state.operation->commitToken;
  Virtualizer::update(container, ownWrite(container, keys));
  Virtualizer::update(container, echo(container, keys, token));
  CHECK(!container.state.operation);

  // A report built on the state before the command, then one built on the state after it.
  container.requestScrollToRow(0, 0.0, UNDEFINED_INDEX);
  Virtualizer::update(container, report(keys, target));
  container.requestScrollToRow(30, 1.0, UNDEFINED_INDEX);
  // The reader has scrolled away in the meantime.
  FrameInput away = report(keys, target + 400.0);
  away.userScrolled = true;
  Virtualizer::update(container, away);
  Virtualizer::update(container, report(keys, target + 400.0));
  CHECK(!container.state.operation);
  CHECK_NEAR(container.revision.offsetY, target + 400.0, 0.5);

  // A newer sequence still runs, even for the same index.
  container.requestScrollToRow(30, 2.0, UNDEFINED_INDEX);
  Virtualizer::update(container, report(keys, target + 400.0));
  CHECK(container.state.operation && container.state.operation->type == OperationType::ScrollToKey);
}

namespace {

/*
 * The keys after a drag moves the row at from to the index to, like arrayMove in JS.
 */
std::vector<std::string> movedKeys(std::vector<std::string> keys, std::size_t from, std::size_t to) {
  std::string key = keys[from];
  keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(from));
  keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(to), key);
  return keys;
}

/*
 * The commit that fires onDragEnd. The keys have not changed yet.
 */
FrameInput drop(
  const std::vector<std::string>& keys,
  double offset,
  const std::string& key,
  double header = HEADER,
  DropHint::Kind kind = DropHint::Kind::Moved) {
  FrameInput input = report(keys, offset, header);
  input.dropHint = DropHint{kind, key};
  return input;
}

}

/*
 * The Reorder screen opens at the top with a header and only the first rows measured. The top
 * row is dragged six rows down. The drag already showed the new order at offset 0. The commit
 * that moves the row keeps offset 0 instead of following the row it held on screen, also once
 * the rows below are measured and the estimated total shrinks to the real one.
 */
TEST(dropping_the_top_row_lower_keeps_the_offset_while_estimates_correct) {
  constexpr double SCREEN_HEADER = 58.0;
  constexpr double SCREEN_ROW = 67.0;
  std::vector<std::string> keys = keysFor(80);
  Container container;
  Virtualizer::update(container, report(keys, 0.0, SCREEN_HEADER));
  layoutPass(container, SCREEN_HEADER, 0, 11, SCREEN_ROW);
  Virtualizer::update(container, report(keys, 0.0, SCREEN_HEADER));
  CHECK_EQ(container.state.anchor.key, std::string("k0"));
  double estimatedTotal = container.revision.contentHeight;

  Virtualizer::update(container, drop(keys, 0.0, "k0", SCREEN_HEADER));
  std::vector<std::string> reordered = movedKeys(keys, 0, 6);
  Virtualizer::update(container, report(reordered, 0.0, SCREEN_HEADER));
  CHECK_NEAR(container.revision.offsetY, 0.0, 0.5);
  CHECK(!container.state.offsetCorrected);

  // The rows below get measured in the same settle. The total shrinks toward the real one.
  layoutPass(container, SCREEN_HEADER, 0, 79, SCREEN_ROW);
  CHECK(container.revision.contentHeight < estimatedTotal);
  for (int commit = 0; commit < 3; ++commit) {
    Virtualizer::update(container, report(reordered, 0.0, SCREEN_HEADER));
  }
  CHECK_NEAR(container.revision.offsetY, 0.0, 0.5);
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(onScreen(container, "k0"), SCREEN_HEADER + 6 * SCREEN_ROW, 0.5);
}

/*
 * Scrolled into the list, the row cut by the top edge is the anchor. Dragged three rows down,
 * the drop keeps the offset, which is where the drag showed the rows.
 */
TEST(dropping_the_anchor_row_lower_keeps_the_offset_when_scrolled) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, 59, 100.0);
  double offset = HEADER + 10 * 100.0 + 40.0;
  Virtualizer::update(container, report(keys, offset));
  Virtualizer::update(container, report(keys, offset));
  CHECK_EQ(container.state.anchor.key, std::string("k10"));

  Virtualizer::update(container, drop(keys, offset, "k10"));
  std::vector<std::string> reordered = movedKeys(keys, 10, 13);
  Virtualizer::update(container, report(reordered, offset));
  layoutPass(container, HEADER, 0, 59, 100.0);
  Virtualizer::update(container, report(reordered, offset));
  CHECK_NEAR(container.revision.offsetY, offset, 0.5);
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(onScreen(container, "k10"), 3 * 100.0 - 40.0, 0.5);
}

/*
 * A row from below dropped into the slot of the anchor row pushes the anchor down by one row.
 * Holding the anchor would scroll the dropped row out of view. The drop keeps the offset.
 */
TEST(dropping_a_row_into_the_anchor_slot_keeps_the_offset) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, 59, 100.0);
  double offset = HEADER + 10 * 100.0 + 40.0;
  Virtualizer::update(container, report(keys, offset));
  Virtualizer::update(container, report(keys, offset));
  CHECK_EQ(container.state.anchor.key, std::string("k10"));

  Virtualizer::update(container, drop(keys, offset, "k15"));
  std::vector<std::string> reordered = movedKeys(keys, 15, 10);
  Virtualizer::update(container, report(reordered, offset));
  CHECK_NEAR(container.revision.offsetY, offset, 0.5);
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(onScreen(container, "k15"), -40.0, 0.5);
}

/*
 * Only a drop keeps the offset. A reorder from data, or one after a drop that moved nothing,
 * still holds the row on screen. The drop is used up by the first key change.
 */
TEST(a_reorder_without_a_pending_drop_still_holds_the_anchor_row) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, 59, 100.0);
  double offset = HEADER + 10 * 100.0 + 40.0;
  Virtualizer::update(container, report(keys, offset));
  Virtualizer::update(container, report(keys, offset));

  // A drop that moved nothing, then an append that does not move the dropped row.
  Virtualizer::update(container, drop(keys, offset, "k10"));
  std::vector<std::string> appended = keys;
  appended.push_back("extra");
  Virtualizer::update(container, report(appended, offset));
  CHECK(container.state.pendingDrop.kind == DropHint::Kind::None);
  CHECK_NEAR(container.revision.offsetY, offset, 0.5);

  double k10Before = onScreen(container, "k10");
  std::vector<std::string> reordered = movedKeys(appended, 10, 13);
  Virtualizer::update(container, report(reordered, offset));
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(onScreen(container, "k10"), k10Before, 0.5);
}

namespace {

/*
 * A list of rows 100 tall, scrolled 40 into k10, which is the anchor.
 */
void scrollIntoK10(Container& container, const std::vector<std::string>& keys, double& offset) {
  Virtualizer::update(container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, keys.size() - 1, 100.0);
  offset = HEADER + 10 * 100.0 + 40.0;
  Virtualizer::update(container, report(keys, offset));
  Virtualizer::update(container, report(keys, offset));
}

}

/*
 * A card dropped from another column of a drag board at the top of a column at rest. The
 * target opened a gap under the header for it. The commit that inserts it keeps offset 0, so
 * the card lands under the header instead of the column scrolling one row down.
 */
TEST(a_row_landing_at_the_top_from_another_list_keeps_the_offset_at_rest) {
  std::vector<std::string> keys = keysFor(30);
  Container container;
  Virtualizer::update(container, report(keys, 0.0));
  layoutPass(container, HEADER, 0, 29, 100.0);
  Virtualizer::update(container, report(keys, 0.0));
  Virtualizer::update(container, drop(keys, 0.0, "incoming", HEADER, DropHint::Kind::Inserted));
  std::vector<std::string> next = keys;
  next.insert(next.begin(), "incoming");
  Virtualizer::update(container, report(next, 0.0));
  layoutPass(container, HEADER, 0, 30, 100.0);
  Virtualizer::update(container, report(next, container.revision.offsetY));
  CHECK_NEAR(container.revision.offsetY, 0.0, 0.5);
  CHECK_NEAR(onScreen(container, "incoming"), HEADER, 0.5);
  CHECK_NEAR(onScreen(container, "k0"), HEADER + 100.0, 0.5);
}

/*
 * Scrolled into the target, a card dropped into the anchor row's slot takes the place the gap
 * showed, 40 above the top edge, instead of the column holding k10 and pushing the card a row
 * further up out of view.
 */
TEST(a_row_landing_in_the_anchor_slot_from_another_list_keeps_the_offset) {
  std::vector<std::string> keys = keysFor(60);
  double offset = 0.0;
  Container container;
  scrollIntoK10(container, keys, offset);
  CHECK_EQ(container.state.anchor.key, std::string("k10"));
  Virtualizer::update(container, drop(keys, offset, "incoming", HEADER, DropHint::Kind::Inserted));
  std::vector<std::string> next = keys;
  next.insert(next.begin() + 10, "incoming");
  Virtualizer::update(container, report(next, offset));
  layoutPass(container, HEADER, 0, 60, 100.0);
  Virtualizer::update(container, report(next, container.revision.offsetY));
  CHECK_NEAR(container.revision.offsetY, offset, 0.5);
  CHECK_NEAR(onScreen(container, "incoming"), -40.0, 0.5);
  CHECK_NEAR(onScreen(container, "k10"), 60.0, 0.5);
}

/*
 * The top card of a scrolled column dragged out to another list. The home list closed the gap
 * while the card floated, which showed k11 at -40. The commit that removes the card keeps the
 * offset there instead of holding the next row and moving it down a row.
 */
TEST(a_row_leaving_for_another_list_keeps_the_offset_of_the_closed_gap) {
  std::vector<std::string> keys = keysFor(60);
  double offset = 0.0;
  Container container;
  scrollIntoK10(container, keys, offset);
  double k11Resting = onScreen(container, "k11");
  Virtualizer::update(container, drop(keys, offset, "k10", HEADER, DropHint::Kind::Removed));
  std::vector<std::string> next = keys;
  next.erase(next.begin() + 10);
  Virtualizer::update(container, report(next, offset));
  CHECK_NEAR(container.revision.offsetY, offset, 0.5);
  CHECK_NEAR(onScreen(container, "k11"), k11Resting - 100.0, 0.5);
}

/*
 * JS rejected the drop: no key change came within DROP_HINT_TIMEOUT_MS. A later server sort
 * that happens to move that same row holds the anchor like any reorder from data. A props
 * change with the same keys, like the mounted range moving, does not end the hint early.
 */
TEST(a_rejected_drop_then_a_server_sort_holds_the_anchor_row) {
  std::vector<std::string> keys = keysFor(60);
  double offset = 0.0;
  Container container;
  scrollIntoK10(container, keys, offset);
  FrameInput dropped = drop(keys, offset, "k10");
  dropped.frameTimeMs = 1000.0;
  Virtualizer::update(container, dropped);
  FrameInput rangeMoved = report(keys, offset);
  rangeMoved.frameTimeMs = 1100.0;
  Virtualizer::update(container, rangeMoved);
  CHECK(container.state.pendingDrop.kind == DropHint::Kind::Moved);
  FrameInput idle = report(keys, offset);
  idle.frameTimeMs = 1000.0 + DROP_HINT_TIMEOUT_MS + 500.0;
  Virtualizer::update(container, idle);
  CHECK(container.state.pendingDrop.kind == DropHint::Kind::None);

  double k10Before = onScreen(container, "k10");
  FrameInput sorted = report(movedKeys(keys, 10, 13), offset);
  sorted.frameTimeMs = idle.frameTimeMs + 5000.0;
  Virtualizer::update(container, sorted);
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(onScreen(container, "k10"), k10Before, 0.5);
}

/*
 * A drop hint no key change used expires DROP_HINT_TIMEOUT_MS after it came in.
 */
TEST(a_drop_hint_expires_after_its_timeout) {
  std::vector<std::string> keys = keysFor(60);
  double offset = 0.0;
  Container container;
  scrollIntoK10(container, keys, offset);
  FrameInput dropped = drop(keys, offset, "k10");
  dropped.frameTimeMs = 1000.0;
  Virtualizer::update(container, dropped);
  FrameInput soon = report(keys, offset);
  soon.frameTimeMs = 1000.0 + DROP_HINT_TIMEOUT_MS / 2.0;
  Virtualizer::update(container, soon);
  CHECK(container.state.pendingDrop.kind == DropHint::Kind::Moved);
  FrameInput late = report(keys, offset);
  late.frameTimeMs = 1000.0 + DROP_HINT_TIMEOUT_MS + 1.0;
  Virtualizer::update(container, late);
  CHECK(container.state.pendingDrop.kind == DropHint::Kind::None);

  double k10Before = onScreen(container, "k10");
  Virtualizer::update(container, report(movedKeys(keys, 10, 13), offset));
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(onScreen(container, "k10"), k10Before, 0.5);
}

/*
 * The first key change after a drop moved the dropped row and also swapped two other rows,
 * like a sort that ran with the move. That is not the change the drag showed. The anchor
 * holds, and the hint is used up.
 */
TEST(a_permutation_of_several_rows_after_a_drop_holds_the_anchor_row) {
  std::vector<std::string> keys = keysFor(60);
  double offset = 0.0;
  Container container;
  scrollIntoK10(container, keys, offset);
  Virtualizer::update(container, drop(keys, offset, "k10"));
  std::vector<std::string> sorted = movedKeys(keys, 10, 13);
  std::swap(sorted[20], sorted[21]);
  double k10Before = onScreen(container, "k10");
  Virtualizer::update(container, report(sorted, offset));
  CHECK(container.state.pendingDrop.kind == DropHint::Kind::None);
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(onScreen(container, "k10"), k10Before, 0.5);
}

/*
 * A removal hint only covers the one row. A change that removed another row too holds the
 * anchor's fallback like any removal.
 */
TEST(a_removal_of_more_than_the_dragged_row_is_not_the_drop) {
  std::vector<std::string> keys = keysFor(60);
  double offset = 0.0;
  Container container;
  scrollIntoK10(container, keys, offset);
  Virtualizer::update(container, drop(keys, offset, "k10", HEADER, DropHint::Kind::Removed));
  std::vector<std::string> next = keys;
  next.erase(next.begin() + 10);
  next.erase(next.begin() + 2);
  double k11Before = onScreen(container, "k11");
  Virtualizer::update(container, report(next, offset));
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(onScreen(container, "k11"), k11Before, 0.5);
}

/*
 * The reader rests mid list. The rows after the screen are still estimated, and they are
 * much taller than the estimate. A change removes the measured rows below the top row.
 * For one update the estimated total is shorter than the offset. The layout pass of the
 * same commit measures the rows that moved in, and the real total has room for the offset.
 * The top row stays where it was instead of the view running to the end.
 */
TEST(removal_below_the_top_row_keeps_it_when_the_rows_moving_in_measure_taller) {
  constexpr double TALL = 800.0;
  std::vector<std::string> keys = keysFor(16);
  Container container;
  Virtualizer::update(container, report(keys, 0.0, 0.0));
  // Rows 0 to 12 get real sizes. Rows 13 to 15 stay estimated.
  layoutPass(container, 0.0, 0, 12, ESTIMATED_ROW_HEIGHT);

  double resting = 5 * ESTIMATED_ROW_HEIGHT;
  Virtualizer::update(container, report(keys, resting, 0.0));
  Virtualizer::update(container, report(keys, resting, 0.0));
  CHECK_NEAR(onScreen(container, "k5"), 0.0, 0.5);
  CHECK(!container.state.operation);

  // Rows 6 to 12 go. The estimated rows 13 to 15 now follow the top row.
  std::vector<std::string> removed(keys.begin(), keys.begin() + 6);
  removed.insert(removed.end(), keys.begin() + 13, keys.end());
  Virtualizer::update(container, report(removed, resting, 0.0));
  layoutPass(container, 0.0, 6, 8, TALL);
  CHECK_NEAR(onScreen(container, "k5"), 0.0, 0.5);

  for (int commit = 0; commit < 4; ++commit) {
    std::uint64_t current = container.state.operation ? container.state.operation->commitToken : 0;
    if (container.state.offsetCorrected) {
      Virtualizer::update(container, ownWrite(container, removed, 0.0));
      layoutPass(container, 0.0, 6, 8, TALL);
    }
    Virtualizer::update(container, echo(container, removed, current, 0.0));
    layoutPass(container, 0.0, 6, 8, TALL);
  }
  CHECK_NEAR(onScreen(container, "k5"), 0.0, 0.5);
  CHECK_NEAR(container.revision.offsetY, resting, 0.5);
  CHECK(!container.state.offsetCorrected);
}

namespace {

/*
 * A list of 12 rows resting at its end with only rows 4 to 11 measured gets a scroll to the
 * start. The host applies it and reports back, then the rows the jump brought in measure taller
 * than their estimate. Returns the offset the list rests at.
 */
double scrollToStartThenMeasureTheTopRows(bool inverted, double header) {
  std::vector<std::string> keys = keysFor(12);
  Container container;
  auto frame = [&](double offset) {
    FrameInput input = report(keys, offset, header);
    input.inverted = inverted;
    return input;
  };
  Virtualizer::update(container, frame(0.0));
  layoutPass(container, header, 4, 11, ESTIMATED_ROW_HEIGHT);
  double end = container.revision.contentHeight - WINDOW_HEIGHT;
  for (int commit = 0; commit < 4; ++commit) {
    Virtualizer::update(container, frame(end));
  }
  CHECK_NEAR(container.revision.offsetY, end, 0.5);
  CHECK(!container.revision.rows[0].measured);

  container.scrollToStart();
  Virtualizer::update(container, frame(end));
  CHECK(container.state.operation && container.state.operation->type == OperationType::ScrollToStart);
  for (int commit = 0; commit < 3; ++commit) {
    FrameInput landed = frame(container.revision.offsetY);
    landed.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, landed);
  }
  CHECK(!container.state.operation);
  CHECK_NEAR(container.revision.offsetY, 0.0, 0.5);

  layoutPass(container, header, 0, 3, 200.0);
  for (int commit = 0; commit < 4; ++commit) {
    Virtualizer::update(container, frame(container.revision.offsetY));
  }
  return container.revision.offsetY;
}

}

/*
 * A list resting at its start holds the top row, guessed size or not. Nothing sits above that
 * row, and holding a measured row further down would push the view off the start when the top
 * rows measure taller.
 */
TEST(list_at_its_start_stays_there_when_the_top_rows_measure_taller) {
  CHECK_NEAR(scrollToStartThenMeasureTheTopRows(false, 0.0), 0.0, 0.5);
}

TEST(list_at_its_start_below_a_header_stays_there_when_the_top_rows_measure_taller) {
  CHECK_NEAR(scrollToStartThenMeasureTheTopRows(false, HEADER), 0.0, 0.5);
}

/*
 * The same for an inverted list a scroll to the start took off its bottom.
 */
TEST(inverted_list_at_its_start_stays_there_when_the_top_rows_measure_taller) {
  CHECK_NEAR(scrollToStartThenMeasureTheTopRows(true, 0.0), 0.0, 0.5);
}

namespace {

constexpr double SHORT_ROW = 60.0;

/*
 * A list of rows SHORT_ROW tall below a header of 100, resting at its start, gets two rows
 * prepended. The correction holds row k0 where the reader had it, 100 below the top. The layout
 * pass measures the first new row and leaves the second, which is on screen above k0, for later.
 * Returns the keys after the prepend.
 */
std::vector<std::string> prependTwoRowsAndMeasureTheFirst(Container& container) {
  std::vector<std::string> keys = keysFor(30);
  Virtualizer::update(container, report(keys, 0.0, 100.0));
  layoutPass(container, 100.0, 0, 29, SHORT_ROW);
  Virtualizer::update(container, report(keys, 0.0, 100.0));
  CHECK_NEAR(onScreen(container, "k0"), 100.0, 0.5);

  std::vector<std::string> next = keysFor(2, "p");
  next.insert(next.end(), keys.begin(), keys.end());
  Virtualizer::update(container, report(next, 0.0, 100.0));
  CHECK(container.state.operation && container.state.operation->type == OperationType::MaintainAnchor);
  layoutPass(container, 100.0, 0, 0, SHORT_ROW);
  CHECK_NEAR(onScreen(container, "k0"), 100.0, 0.5);
  CHECK(!container.revision.rows[1].measured);
  return next;
}

}

/*
 * The host applies a prepend's correction and reports back, which ends it. The top of the
 * viewport is now a measured new row, and the new row between it and the reader's row has no
 * size yet. That row comes in taller. The reader's row stays, and the rows above it take the
 * growth.
 */
TEST(a_prepended_row_measured_after_its_correction_ended_leaves_the_readers_row_in_place) {
  Container container;
  std::vector<std::string> next = prependTwoRowsAndMeasureTheFirst(container);

  std::uint64_t token = container.state.operation->commitToken;
  Virtualizer::update(container, ownWrite(container, next, 100.0));
  Virtualizer::update(container, echo(container, next, token, 100.0));
  CHECK(!container.state.operation);
  CHECK_NEAR(onScreen(container, "p0"), -20.0, 0.5);

  layoutPass(container, 100.0, 1, 1, 160.0);
  CHECK_NEAR(onScreen(container, "k0"), 100.0, 0.5);
  Virtualizer::update(container, ownWrite(container, next, 100.0));
  Virtualizer::update(container, echo(container, next, 0, 100.0));
  CHECK_NEAR(onScreen(container, "k0"), 100.0, 0.5);
}

/*
 * The same when a finger is down. The core sees the report from before the write a second
 * time, and then the finger's frame reports the correction back, which ends it there.
 */
TEST(a_prepended_row_measured_after_a_finger_ended_its_correction_leaves_the_readers_row_in_place) {
  Container container;
  std::vector<std::string> next = prependTwoRowsAndMeasureTheFirst(container);

  std::uint64_t token = container.state.operation->commitToken;
  Virtualizer::update(container, report(next, 0.0, 100.0));
  CHECK(container.state.operation && container.state.operation->commitToken == token);
  FrameInput finger = echo(container, next, token, 100.0);
  finger.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, finger);
  CHECK(!container.state.operation);

  layoutPass(container, 100.0, 1, 1, 160.0);
  CHECK_NEAR(onScreen(container, "k0"), 100.0, 0.5);
}

namespace {

/*
 * A list of 40 rows resting on row k10 with rows 0 to 4 not measured yet. The layout pass
 * measures row 3 taller by 80 and moves the offset to keep k10 at the top. No operation runs.
 * Returns the offset the list rested at before.
 */
double measureARowAboveAtRest(Container& container, const std::vector<std::string>& keys) {
  Virtualizer::update(container, report(keys, 0.0, 0.0));
  layoutPass(container, 0.0, 5, 30, ESTIMATED_ROW_HEIGHT);
  double resting = 10 * ESTIMATED_ROW_HEIGHT;
  Virtualizer::update(container, report(keys, resting, 0.0));
  Virtualizer::update(container, report(keys, resting, 0.0));
  CHECK_NEAR(onScreen(container, "k10"), 0.0, 0.5);

  layoutPass(container, 0.0, 3, 3, ESTIMATED_ROW_HEIGHT + 80.0);
  CHECK(container.state.offsetCorrected);
  CHECK(!container.state.operation);
  CHECK_NEAR(container.revision.offsetY, resting + 80.0, 0.5);
  return resting;
}

}

/*
 * A report from before the write of that correction reaches the core after it. The view is
 * where the write put it. The core keeps the reader's row and asks for the same offset again
 * as an operation, and a row measured in that frame's layout pass moves the offset from there.
 */
TEST(a_report_from_before_a_layout_pass_correction_keeps_the_readers_row) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  double resting = measureARowAboveAtRest(container, keys);

  Virtualizer::update(container, report(keys, resting, 0.0));
  CHECK(container.state.operation && container.state.operation->type == OperationType::MaintainAnchor);
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(container.revision.offsetY, resting + 80.0, 0.5);

  layoutPass(container, 0.0, 2, 2, ESTIMATED_ROW_HEIGHT + 80.0);
  CHECK_NEAR(container.revision.offsetY, resting + 160.0, 0.5);
  CHECK_NEAR(onScreen(container, "k10"), 0.0, 0.5);

  std::uint64_t token = container.state.operation->commitToken;
  Virtualizer::update(container, echo(container, keys, token, 0.0));
  CHECK(!container.state.operation);
  CHECK_NEAR(onScreen(container, "k10"), 0.0, 0.5);
}

/*
 * The commit that carries the first correction measures another row and moves the offset
 * again. The host's report of the first write arrives after that. It shows an offset the view
 * passed on its way, and the reader's row stays where the second correction put it.
 */
TEST(a_report_of_an_earlier_layout_pass_correction_keeps_the_readers_row) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  double resting = measureARowAboveAtRest(container, keys);

  Virtualizer::update(container, ownWrite(container, keys, 0.0));
  layoutPass(container, 0.0, 2, 2, ESTIMATED_ROW_HEIGHT + 80.0);
  CHECK(!container.state.operation);
  CHECK_NEAR(container.revision.offsetY, resting + 160.0, 0.5);

  Virtualizer::update(container, report(keys, resting + 80.0, 0.0));
  CHECK(container.state.operation && container.state.operation->type == OperationType::MaintainAnchor);
  CHECK_NEAR(container.revision.offsetY, resting + 160.0, 0.5);
  CHECK_NEAR(onScreen(container, "k10"), 0.0, 0.5);
}

/*
 * The host reports the corrected offset, and later the view is moved back to where it was,
 * like a scrollToOffset from the app. The first report used the remembered offset up. The
 * second is where the view is, and nothing pulls it back.
 */
TEST(a_view_moved_back_after_a_reported_layout_pass_correction_stays_where_it_was_moved) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  double resting = measureARowAboveAtRest(container, keys);

  Virtualizer::update(container, ownWrite(container, keys, 0.0));
  Virtualizer::update(container, report(keys, resting + 80.0, 0.0));
  CHECK(!container.state.operation);

  Virtualizer::update(container, report(keys, resting, 0.0));
  CHECK(!container.state.operation);
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(container.revision.offsetY, resting, 0.5);
}

/*
 * A finger moves the view while the correction's write is on its way. The host adds a
 * correction to a moving view, and asking for the offset again would add it twice. The report
 * is taken as it is.
 */
TEST(a_finger_report_from_before_a_layout_pass_correction_starts_no_operation) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  double resting = measureARowAboveAtRest(container, keys);

  FrameInput finger = report(keys, resting, 0.0);
  finger.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, finger);
  CHECK(!container.state.operation);
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(container.revision.offsetY, resting, 0.5);
}

namespace {

/*
 * A list of 60 rows of 100 loses its last five rows while a finger holds it at offset 5000.
 * The view is past the new end, and a clamp to the end starts. Returns the keys left.
 */
std::vector<std::string> shrinkUnderAFinger(Container& container) {
  std::vector<std::string> keys = keysFor(60);
  Virtualizer::update(container, report(keys, 0.0, 0.0));
  layoutPass(container, 0.0, 0, 59, 100.0);
  FrameInput finger = report(keys, 5000.0, 0.0);
  finger.scrollPhase = ScrollPhase::Dragging;
  finger.userScrolled = true;
  Virtualizer::update(container, finger);

  std::vector<std::string> shrunk(keys.begin(), keys.begin() + 55);
  finger = report(shrunk, 5000.0, 0.0);
  finger.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, finger);
  CHECK(container.state.operation && container.state.operation->type == OperationType::ShrinkClamp);
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 0.5);
  return shrunk;
}

}

/*
 * The finger's report from before the clamp's write reaches the core after it. The clamp keeps
 * its token. The host then sees the correction it already added to the view, not a new one.
 */
TEST(a_finger_report_from_before_a_shrink_clamp_keeps_the_clamp_and_its_token) {
  Container container;
  std::vector<std::string> shrunk = shrinkUnderAFinger(container);
  std::uint64_t token = container.state.operation->commitToken;
  double end = container.revision.contentHeight - WINDOW_HEIGHT;

  FrameInput finger = report(shrunk, 5000.0, 0.0);
  finger.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, finger);
  CHECK(container.state.operation && container.state.operation->type == OperationType::ShrinkClamp);
  CHECK_EQ(container.state.operation->commitToken, token);
  CHECK(container.state.offsetCorrected);
  CHECK_NEAR(container.revision.offsetY, end, 0.5);

  // The finger's frame that reports the clamp back ends it, like any correction a finger takes over.
  finger = report(shrunk, end, 0.0);
  finger.scrollPhase = ScrollPhase::Dragging;
  finger.commitToken = token;
  Virtualizer::update(container, finger);
  CHECK(!container.state.operation);
}

/*
 * The host added the clamp to the moving view, which came to rest short of the end, and
 * reports the clamp back from there. The clamp is done. It does not keep asking for the end.
 */
TEST(a_shrink_clamp_made_under_a_finger_ends_when_the_resting_view_reports_it_back) {
  Container container;
  std::vector<std::string> shrunk = shrinkUnderAFinger(container);
  std::uint64_t token = container.state.operation->commitToken;
  double rest = container.revision.contentHeight - WINDOW_HEIGHT - 260.0;

  FrameInput resting = report(shrunk, rest, 0.0);
  resting.commitToken = token;
  Virtualizer::update(container, resting);
  CHECK(!container.state.operation);
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(container.revision.offsetY, rest, 0.5);
}

/*
 * The host applied the first clamp before another measurement moved its target. A delayed
 * finger report of that first write keeps the operation until the newer write reports back.
 */
TEST(a_finger_echo_of_an_earlier_clamp_revision_keeps_the_pending_correction) {
  Container container;
  std::vector<std::string> keys = shrinkUnderAFinger(container);
  auto publish = [&]() {
    return container.resolveStateUpdate(0.0, 5000.0, container.revision.contentWidth,
      container.revision.contentHeight);
  };
  ContainerStateUpdate first = publish();
  CHECK(first.commitToken != 0);
  CHECK(first.commitRevision != 0);
  CHECK_EQ(publish().commitRevision, first.commitRevision);

  layoutPass(container, 0.0, 54, 54, 28.0);
  ContainerStateUpdate second = publish();
  CHECK_EQ(second.commitToken, first.commitToken);
  CHECK(second.commitRevision > first.commitRevision);
  CHECK_NEAR(first.offsetY - second.offsetY, 72.0, 0.5);

  FrameInput finger = report(keys, first.offsetY, 0.0);
  finger.scrollPhase = ScrollPhase::Dragging;
  finger.commitToken = first.commitToken;
  finger.commitRevision = first.commitRevision;
  Virtualizer::update(container, finger);
  CHECK(container.state.operation);
  CHECK_EQ(container.state.operation->commitToken, first.commitToken);
  CHECK_NEAR(container.revision.offsetY, second.offsetY, 0.5);

  finger.offsetY = second.offsetY;
  finger.commitRevision = second.commitRevision;
  Virtualizer::update(container, finger);
  CHECK(!container.state.operation);
  CHECK(!container.state.offsetCorrected);
}

/*
 * A finger moves before the prepend's write. The target follows that travel once, and the
 * host still adds only the prepend's correction to the moving view.
 */
TEST(a_revisioned_prepend_keeps_the_correction_while_a_finger_moves_before_its_write) {
  Container container;
  std::vector<std::string> keys = keysFor(40);
  Virtualizer::update(container, report(keys, 0.0, 0.0));
  layoutPass(container, 0.0, 0, 39, 100.0);
  Virtualizer::update(container, report(keys, 1000.0, 0.0));
  keys.insert(keys.begin(), "prepended");
  Virtualizer::update(container, report(keys, 1000.0, 0.0));
  ContainerStateUpdate first = container.resolveStateUpdate(0.0, 1000.0,
    container.revision.contentWidth, container.revision.contentHeight);
  CHECK(first.commitToken != 0);

  FrameInput finger = report(keys, 1030.0, 0.0);
  finger.scrollPhase = ScrollPhase::Dragging;
  finger.userScrolled = true;
  Virtualizer::update(container, finger);
  ContainerStateUpdate moved = container.resolveStateUpdate(0.0, 1030.0,
    container.revision.contentWidth, container.revision.contentHeight);
  CHECK_EQ(moved.commitToken, first.commitToken);
  CHECK(moved.commitRevision > first.commitRevision);
  CHECK_NEAR(moved.offsetY - 1030.0, first.offsetY - 1000.0, 0.5);

  Virtualizer::update(container, finger);
  ContainerStateUpdate repeated = container.resolveStateUpdate(0.0, 1030.0,
    container.revision.contentWidth, container.revision.contentHeight);
  CHECK_EQ(repeated.commitRevision, moved.commitRevision);
  CHECK_NEAR(repeated.offsetY, moved.offsetY, 0.5);

  finger.offsetY = moved.offsetY;
  finger.commitToken = moved.commitToken;
  finger.commitRevision = moved.commitRevision;
  Virtualizer::update(container, finger);
  CHECK(!container.state.operation);
}

/*
 * A late measurement corrects the view after a clamp was acknowledged. An older echo of
 * that clamp must not start another one. A real finger move with that revision still can.
 */
TEST(an_earlier_write_report_starts_no_second_clamp_but_a_finger_move_still_does) {
  for (bool userScrolled : {false, true}) {
    Container container;
    std::vector<std::string> keys = shrinkUnderAFinger(container);
    ContainerStateUpdate first = container.resolveStateUpdate(0.0, 5000.0,
      container.revision.contentWidth, container.revision.contentHeight);
    FrameInput finger = report(keys, first.offsetY, 0.0);
    finger.scrollPhase = ScrollPhase::Dragging;
    finger.commitToken = first.commitToken;
    finger.commitRevision = first.commitRevision;
    Virtualizer::update(container, finger);
    CHECK(!container.state.operation);

    layoutPass(container, 0.0, 0, 0, 28.0);
    ContainerStateUpdate measured = container.resolveStateUpdate(0.0, first.offsetY,
      container.revision.contentWidth, container.revision.contentHeight);
    CHECK(measured.applyOffset);
    CHECK_EQ(measured.commitToken, static_cast<std::uint64_t>(0));
    CHECK(measured.commitRevision > first.commitRevision);
    CHECK_EQ(container.resolveStateUpdate(0.0, first.offsetY, container.revision.contentWidth,
      container.revision.contentHeight).commitRevision, measured.commitRevision);

    finger.userScrolled = userScrolled;
    Virtualizer::update(container, finger);
    if (userScrolled) {
      CHECK(container.state.operation);
      CHECK(container.state.operation->type == OperationType::ShrinkClamp);
      CHECK(container.state.offsetCorrected);
    } else {
      CHECK(!container.state.operation);
      CHECK(!container.state.offsetCorrected);
    }
  }
}

/*
 * A real finger cancels a command that is still landing and keeps its own offset.
 */
TEST(a_finger_move_owns_the_offset_when_it_interrupts_a_scroll_to_end) {
  Container container;
  std::vector<std::string> keys = keysFor(60);
  Virtualizer::update(container, report(keys, 0.0, 0.0));
  layoutPass(container, 0.0, 0, 59, 100.0);
  container.scrollToEnd();
  Virtualizer::update(container, report(keys, 0.0, 0.0));
  CHECK(container.state.pendingScrollToEnd);
  ContainerStateUpdate command = container.resolveStateUpdate(0.0, 0.0,
    container.revision.contentWidth, container.revision.contentHeight);

  FrameInput finger = report(keys, 1000.0, 0.0);
  finger.userScrolled = true;
  finger.scrollPhase = ScrollPhase::Dragging;
  finger.commitRevision = command.commitRevision - 1;
  Virtualizer::update(container, finger);
  CHECK(!container.state.pendingScrollToEnd);
  CHECK(!container.state.operation);
  CHECK(!container.state.offsetCorrected);
  CHECK_NEAR(container.revision.offsetY, 1000.0, 0.5);
}

TEST(the_first_correction_at_the_origin_has_a_stable_publication_revision) {
  Container container;
  container.state.offsetCorrected = true;
  ContainerStateUpdate first = container.resolveStateUpdate(0.0, 0.0, 0.0, 0.0);
  CHECK(first.commitRevision != 0);
  CHECK_EQ(first.commitToken, static_cast<std::uint64_t>(0));
  CHECK_EQ(container.resolveStateUpdate(0.0, 0.0, 0.0, 0.0).commitRevision, first.commitRevision);
}

/*
 * An inverted list rests on its bottom with only the rows on screen measured. The rows above
 * its two newest rows are removed, and rows without sizes move in from above. The hold of the
 * newest rows rests on the end and the host reports it back. The rows that moved in then come
 * in taller. The hold is still running and follows the end. The list stays on its bottom.
 */
TEST(inverted_hold_on_the_end_waits_for_the_rows_that_moved_in_to_be_measured) {
  std::vector<std::string> keys = keysFor(30);
  Container container;
  auto frame = [&](const std::vector<std::string>& frameKeys, double offset) {
    FrameInput input = report(frameKeys, offset, 0.0);
    input.inverted = true;
    return input;
  };
  Virtualizer::update(container, frame(keys, 0.0));
  layoutPass(container, 0.0, 18, 29, 100.0);
  for (int commit = 0; commit < 4; ++commit) {
    Virtualizer::update(container, frame(keys, container.revision.offsetY));
  }
  double end = container.revision.contentHeight - WINDOW_HEIGHT;
  CHECK(container.state.invertedInitialized);
  CHECK_NEAR(container.revision.offsetY, end, 0.5);
  double newestBefore = onScreen(container, "k28");

  std::vector<std::string> removed(keys.begin(), keys.begin() + 20);
  removed.insert(removed.end(), keys.begin() + 28, keys.end());
  Virtualizer::update(container, frame(removed, end));
  CHECK(container.state.operation && container.state.operation->type == OperationType::MaintainAnchor);
  std::uint64_t token = container.state.operation->commitToken;
  FrameInput landed = frame(removed, container.revision.offsetY);
  landed.commitToken = token;
  Virtualizer::update(container, landed);
  CHECK(container.state.operation && container.state.operation->commitToken == token);

  layoutPass(container, 0.0, 13, 17, 200.0);
  CHECK_NEAR(onScreen(container, "k28"), newestBefore, 0.5);
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 0.5);
  for (int commit = 0; commit < 3; ++commit) {
    landed = frame(removed, container.revision.offsetY);
    landed.commitToken = token;
    Virtualizer::update(container, landed);
  }
  CHECK(!container.state.operation);
  CHECK_NEAR(onScreen(container, "k28"), newestBefore, 0.5);
}

/*
 * The top row has a size and the rows further down the screen have none yet. The top row is
 * the anchor on the frame that scrolls there and on every frame that rests there, when it is
 * the row held so far.
 */
TEST(a_measured_top_row_stays_the_anchor_while_it_is_the_row_held_so_far) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(container, report(keys, 0.0, 0.0));
  layoutPass(container, 0.0, 0, 9, 100.0);
  CHECK(container.revision.rows[9].measured);
  CHECK(!container.revision.rows[10].measured);

  for (int frame = 0; frame < 3; ++frame) {
    Virtualizer::update(container, report(keys, 850.0, 0.0));
    CHECK_EQ(container.state.anchor.key, std::string("k8"));
    CHECK_NEAR(container.state.anchor.offset, 50.0, 0.001);
    CHECK_NEAR(container.revision.offsetY, 850.0, 0.001);
  }

  // The next row down takes over once it is the top row.
  Virtualizer::update(container, report(keys, 950.0, 0.0));
  CHECK_EQ(container.state.anchor.key, std::string("k9"));
  CHECK_NEAR(container.state.anchor.offset, 50.0, 0.001);
}
