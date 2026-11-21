/*
 * Virtualization tests for the scroll position: prepends, inverted bottom pinning and edge
 * callbacks keep the visible content in place.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "VirtualizationHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

// Scroll position

TEST(prepend_keeps_the_visible_row_in_place) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(200);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  // Rest on row 80.
  double offset = offsetOf(container, 80);
  Virtualizer::update(container, inputFor(keys, offset, fixture));
  CHECK_NEAR(offsetOf(container, 80), container.revision.offsetY, 1.0);

  std::vector<std::string> prepended = keysFor(30, "older");
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  Virtualizer::update(container, inputFor(prepended, offset, fixture));

  std::size_t movedIndex = container.indexOfKey("k80");
  CHECK(movedIndex != UNDEFINED_INDEX);
  CHECK_EQ(movedIndex, static_cast<std::size_t>(110));
  // The same row must still be at the top of the screen.
  CHECK_NEAR(offsetOf(container, movedIndex), container.revision.offsetY, 1.0);
}

namespace {

/*
 * Prepend while resting at offset 0 with a header, like the Feed screen. The first row sits
 * below the top by the header size, but it must hold just like it does mid list: the new
 * rows land above the screen and the offset grows by their height.
 */
void checkPrependHoldsFirstVisibleRow(const Fixture& fixture, double headerSize, std::size_t restIndex) {
  std::vector<std::string> keys = keysFor(200);
  Container container;
  auto frame = [&](const std::vector<std::string>& frameKeys, double offset) {
    FrameInput input = inputFor(frameKeys, offset, fixture);
    input.headerSize = headerSize;
    Virtualizer::update(container, input);
  };
  auto currentOffset = [&]() {
    return fixture.horizontal ? container.revision.offsetX : container.revision.offsetY;
  };

  auto measureAll = [&](std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
      Size size = fixture.horizontal ? Size{100.0, WINDOW_HEIGHT} : Size{WINDOW_WIDTH, 100.0};
      Virtualizer::updateRowAtIndex(container, index, size);
    }
  };

  frame(keys, 0.0);
  measureAll(keys.size());
  frame(keys, 0.0);

  double offset = restIndex == 0 ? 0.0 : offsetOf(container, restIndex);
  frame(keys, offset);
  CHECK_NEAR(currentOffset(), offset, 1.0);
  double restKeyScreenPosition = offsetOf(container, restIndex) - offset;

  std::vector<std::string> prepended = keysFor(10, "older");
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  frame(prepended, offset);

  std::size_t movedIndex = container.indexOfKey("k" + std::to_string(restIndex));
  CHECK_EQ(movedIndex, restIndex + 10);
  // The row stays where it was on screen, and the offset grew by the new rows.
  CHECK_NEAR(offsetOf(container, movedIndex) - currentOffset(), restKeyScreenPosition, 1.0);
  CHECK(container.state.offsetCorrected);

  // The host applies the new offset and measures the new rows, and nothing moves.
  double corrected = currentOffset();
  FrameInput confirm = inputFor(prepended, corrected, fixture);
  confirm.headerSize = headerSize;
  confirm.offsetEnabled = true;
  Virtualizer::update(container, confirm);
  measureAll(prepended.size());
  CHECK_NEAR(offsetOf(container, movedIndex) - currentOffset(), restKeyScreenPosition, 1.0);
}

}

TEST(prepend_at_the_top_keeps_the_first_row_in_place) {
  checkPrependHoldsFirstVisibleRow(Fixture{}, 0.0, 0);
}

TEST(prepend_at_the_top_below_a_header_keeps_the_first_row_in_place) {
  checkPrependHoldsFirstVisibleRow(Fixture{}, 150.0, 0);
}

TEST(prepend_mid_list_below_a_header_keeps_the_visible_row_in_place) {
  checkPrependHoldsFirstVisibleRow(Fixture{}, 150.0, 80);
}

TEST(prepend_at_the_start_of_a_horizontal_list_keeps_the_first_column_in_place) {
  Fixture fixture;
  fixture.horizontal = true;
  fixture.estimatedWidth = 120.0;
  fixture.estimatedHeight = WINDOW_HEIGHT;
  checkPrependHoldsFirstVisibleRow(fixture, 150.0, 0);
}

TEST(inverted_list_opens_pinned_to_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(150);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  // Report the offset back each frame, like the host does, until it settles at the bottom.
  double offset = 0.0;
  for (int frame = 0; frame < 6; ++frame) {
    FrameInput input = inputFor(keys, offset, fixture);
    input.offsetEnabled = false;
    Virtualizer::update(container, input);
    offset = container.revision.offsetY;
  }

  double maxOffset = container.revision.contentHeight - WINDOW_HEIGHT;
  CHECK_NEAR(offset, maxOffset, 1.0);
  checkNoRowLost(container, "inverted at rest");
}

/*
 * Tapping the status bar on an inverted list jumps from the bottom to 0 in one step.
 * The mounted rows must follow the new offset.
 */
TEST(inverted_list_jump_to_top_reports_the_first_rows) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(400);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  double offset = 0.0;
  for (int frame = 0; frame < 6; ++frame) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    offset = container.revision.offsetY;
  }

  FrameInput jump = inputFor(keys, 0.0, fixture);
  jump.userScrolled = true;
  jump.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, jump);

  IndexRange measured = container.getMeasuredRange();
  CHECK(measured.low != UNDEFINED_INDEX);
  CHECK_EQ(measured.low, static_cast<std::size_t>(0));
  checkNoRowLost(container, "inverted after jump to top");

  // The jump must stick and not be pulled back to the bottom.
  FrameInput settle = inputFor(keys, 0.0, fixture);
  Virtualizer::update(container, settle);
  CHECK_NEAR(container.revision.offsetY, 0.0, 1.0);
}

/*
 * Switching an inverted chat to another conversation replaces every row. The new set opens
 * on its bottom, like a fresh list, instead of keeping the old offset and showing its oldest rows.
 */
TEST(inverted_dataset_swap_opens_on_the_new_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(30, "a");
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);

  // The reader scrolls up into the history, then the conversation changes under them.
  FrameInput away = inputFor(keys, bottom - 900.0, fixture);
  away.userScrolled = true;
  Virtualizer::update(container, away);
  Virtualizer::update(container, inputFor(keys, bottom - 900.0, fixture));

  std::vector<std::string> other = keysFor(12, "b");
  FrameInput swap = inputFor(other, bottom - 900.0, fixture);
  Virtualizer::update(container, swap);
  measureRows(container, std::vector<double>(other.size(), 100.0));
  double newBottom = settleAtBottom(container, other, fixture);
  CHECK_NEAR(container.revision.offsetY, newBottom, 1.0);
  CHECK(!container.state.invertedBottomReleased);

  // A partial swap that keeps a visible row still holds that row, like any other change.
  FrameInput up = inputFor(other, newBottom - 300.0, fixture);
  up.userScrolled = true;
  Virtualizer::update(container, up);
  Virtualizer::update(container, inputFor(other, newBottom - 300.0, fixture));
  std::string held = container.state.anchor.key;
  double heldScreen = offsetOf(container, container.indexOfKey(held)) - container.revision.offsetY;
  std::vector<std::string> mixed = keysFor(10, "c");
  mixed.push_back(held);
  std::vector<std::string> after = keysFor(10, "d");
  mixed.insert(mixed.end(), after.begin(), after.end());
  Virtualizer::update(container, inputFor(mixed, container.revision.offsetY, fixture));
  CHECK_NEAR(offsetOf(container, container.indexOfKey(held)) - container.revision.offsetY, heldScreen, 1.0);
}

/*
 * The reader scrolled up into the history and the conversation is swapped. The offset the view
 * still reports happens to equal the bottom of the new rows at their estimated size. That report
 * is not the list arriving on its new bottom. The pin holds until the newest row is measured
 * and the view sits on the real bottom.
 */
TEST(inverted_dataset_swap_pins_again_when_the_old_offset_matches_the_estimated_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(30, "a");
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  settleAtBottom(container, keys, fixture);

  // Twelve unmeasured rows are 1440 points tall. Their bottom is at 600 in an 840 point window.
  std::vector<std::string> other = keysFor(12, "b");
  double parked = static_cast<double>(other.size()) * ESTIMATED_ROW_HEIGHT - WINDOW_HEIGHT;
  FrameInput away = inputFor(keys, parked, fixture);
  away.userScrolled = true;
  Virtualizer::update(container, away);
  Virtualizer::update(container, inputFor(keys, parked, fixture));
  CHECK(container.state.invertedBottomReleased);

  Virtualizer::update(container, inputFor(other, parked, fixture));
  // Without this check the test would pass even if the two offsets never met.
  CHECK_NEAR(container.revision.contentHeight - WINDOW_HEIGHT, parked, 0.5);

  // The layout pass measures the new rows taller than their estimate, and the host keeps reporting.
  measureRows(container, std::vector<double>(other.size(), 200.0));
  Virtualizer::recomputeContentSize(container);
  for (int frame = 0; frame < 6; ++frame) {
    Virtualizer::update(container, inputFor(other, container.revision.offsetY, fixture));
  }
  double newBottom = static_cast<double>(other.size()) * 200.0 - WINDOW_HEIGHT;
  CHECK_NEAR(container.revision.contentHeight - WINDOW_HEIGHT, newBottom, 1.0);
  CHECK_NEAR(container.revision.offsetY, newBottom, 1.0);
  CHECK(container.state.invertedInitialized);
}

/*
 * A scroll to the end is still landing when the layout pass measures a row above the viewport
 * larger than its estimate. The rows move down by the difference. The offset must follow the
 * new bottom in the same pass. Waiting for the next frame shows the rows shifted for one frame,
 * then snapped back, which is the jitter seen as an incoming message is followed.
 */
TEST(scroll_to_end_in_flight_follows_growth_measured_in_the_layout_pass) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);

  // The reader scrolls far up, then a new message arrives and the screen asks for the end.
  FrameInput away = inputFor(keys, bottom - 2000.0, fixture);
  away.userScrolled = true;
  Virtualizer::update(container, away);
  Virtualizer::update(container, inputFor(keys, bottom - 2000.0, fixture));
  keys.push_back("k40");
  container.scrollToEnd();
  Virtualizer::update(container, inputFor(keys, bottom - 2000.0, fixture));
  CHECK(container.state.operation && container.state.operation->type == OperationType::ScrollToEnd);
  double target = container.revision.offsetY;
  CHECK_NEAR(target, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);

  // Our write comes back before the host confirms it, and the pass measures rows above the view.
  FrameInput ownWrite = inputFor(keys, target, fixture);
  ownWrite.offsetEnabled = true;
  ownWrite.commitToken = container.state.operation->commitToken;
  Virtualizer::update(container, ownWrite);
  CHECK(container.state.operation);
  Virtualizer::updateRowAtIndex(container, 30, {WINDOW_WIDTH, 160.0});
  Virtualizer::updateRowAtIndex(container, 40, {WINDOW_WIDTH, 130.0});
  Virtualizer::recomputeContentSize(container);
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);
  CHECK(container.state.offsetCorrected);
}

/*
 * A scroll to the end jumps to rows that have no sizes yet. The host measures the last row
 * first and the other rows on screen some passes later, like rows React renders in steps. The
 * command is still running when they come in taller, and the view ends on the real end.
 */
TEST(scroll_to_end_waits_for_the_rows_on_screen_at_the_end) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  layoutRows(container, 0, std::vector<double>(20, ESTIMATED_ROW_HEIGHT));
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  container.scrollToEnd();
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK(container.state.operation && container.state.operation->type == OperationType::ScrollToEnd);
  auto echo = [&]() {
    FrameInput input = inputFor(keys, container.revision.offsetY, fixture);
    input.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, input);
  };

  // Only the last row has its size. The view sits on the end of the estimates for three frames.
  layoutRows(container, 39, std::vector<double>(1, ESTIMATED_ROW_HEIGHT));
  for (int frame = 0; frame < 3; ++frame) {
    echo();
  }
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 0.5);
  CHECK(container.state.pendingScrollToEnd);

  layoutRows(container, 30, std::vector<double>(9, 300.0));
  for (int frame = 0; frame < 3; ++frame) {
    echo();
  }
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 0.5);
  CHECK(!container.state.pendingScrollToEnd);
  CHECK(!container.state.operation.has_value());
}

/*
 * The same for an inverted list that opens on its bottom: the last row is measured first, the
 * rows above it on screen later and taller. The list still rests on its bottom.
 */
TEST(inverted_list_opens_on_its_bottom_when_the_rows_on_screen_are_measured_after_the_last_one) {
  Fixture fixture;
  fixture.inverted = true;
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  auto echo = [&]() {
    FrameInput input = inputFor(keys, container.revision.offsetY, fixture);
    input.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, input);
  };

  layoutRows(container, 39, std::vector<double>(1, ESTIMATED_ROW_HEIGHT));
  for (int frame = 0; frame < 3; ++frame) {
    echo();
  }
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 0.5);
  CHECK(!container.state.invertedInitialized);

  layoutRows(container, 30, std::vector<double>(9, 300.0));
  for (int frame = 0; frame < 3; ++frame) {
    echo();
  }
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 0.5);
  CHECK(container.state.invertedInitialized);
  CHECK(!container.state.operation.has_value());
}

/*
 * A streaming reply taller than the screen sits at the bottom of an inverted list. When the
 * user drags up into it, the list must stop sticking to the bottom and stay that way while
 * the row grows, or the view snaps back to the bottom.
 */
TEST(inverted_bottom_pin_releases_when_the_user_scrolls_up_a_tall_last_row) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = 3000.0;
  measureRows(container, heights);

  double bottom = settleAtBottom(container, keys, fixture);
  CHECK_NEAR(bottom, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);

  // The finger drags 400 points up, still inside the tall last row.
  double dragged = bottom - 400.0;
  FrameInput drag = inputFor(keys, dragged, fixture);
  drag.userScrolled = true;
  Virtualizer::update(container, drag);
  CHECK_NEAR(container.revision.offsetY, dragged, 1.0);

  // The reply keeps streaming while the finger rests, and the view must not move.
  for (int flush = 1; flush <= 5; ++flush) {
    Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 3000.0 + flush * 60.0});
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK_NEAR(container.revision.offsetY, dragged, 1.0);
}

/*
 * The chat setup where only the newest row can be an anchor. A user drag must still stop
 * the list sticking to the bottom, even though that row is the only anchor on screen.
 */
TEST(inverted_bottom_pin_releases_when_only_the_last_row_is_anchorable) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  double bottom = settleAtBottom(container, keys, fixture, allButLast);

  double dragged = bottom - 300.0;
  FrameInput drag = inputFor(keys, dragged, fixture);
  drag.userScrolled = true;
  drag.nonAnchorKeys = allButLast;
  Virtualizer::update(container, drag);
  CHECK_NEAR(container.revision.offsetY, dragged, 1.0);

  for (int flush = 1; flush <= 5; ++flush) {
    Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 80.0});
    FrameInput rest = inputFor(keys, container.revision.offsetY, fixture);
    rest.nonAnchorKeys = allButLast;
    Virtualizer::update(container, rest);
  }
  CHECK_NEAR(container.revision.offsetY, dragged, 1.0);
}

/*
 * Letting go of the bottom is not permanent. Once the user scrolls back down, growth of the
 * last row is followed again.
 */
TEST(inverted_bottom_pin_reengages_once_the_user_returns_to_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = 3000.0;
  measureRows(container, heights);

  double bottom = settleAtBottom(container, keys, fixture);

  FrameInput away = inputFor(keys, bottom - 400.0, fixture);
  away.userScrolled = true;
  Virtualizer::update(container, away);
  // Without this check the test would pass even if the bottom was never let go.
  CHECK(container.state.invertedBottomReleased);

  FrameInput back = inputFor(keys, bottom, fixture);
  back.userScrolled = true;
  Virtualizer::update(container, back);
  CHECK(!container.state.invertedBottomReleased);

  Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 3300.0});
  double offset = container.revision.offsetY;
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    offset = container.revision.offsetY;
  }
  CHECK_NEAR(offset, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);
}

/*
 * A 3 point nudge, like a stray touch or the scroll view settling after a bounce, does not
 * mean the reader left the bottom. Treating it that way would strand them for the whole reply.
 */
TEST(inverted_bottom_pin_ignores_a_nudge_off_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  double bottom = settleAtBottom(container, keys, fixture, allButLast);

  FrameInput nudge = inputFor(keys, bottom - 3.0, fixture);
  nudge.userScrolled = true;
  nudge.nonAnchorKeys = allButLast;
  Virtualizer::update(container, nudge);
  CHECK(!container.state.invertedBottomReleased);

  // Still following. The view moves with the growing last row.
  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 120.0});
    FrameInput rest = inputFor(keys, container.revision.offsetY, fixture);
    rest.nonAnchorKeys = allButLast;
    Virtualizer::update(container, rest);
  }
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);
}

/*
 * A conversation shorter than the screen has its bottom at offset 0. An overscroll bounce
 * looks far above it. Letting go there would stop following before the reply even fills the screen.
 */
TEST(inverted_bottom_pin_survives_a_bounce_on_a_list_shorter_than_the_viewport) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(3);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  settleAtBottom(container, keys, fixture, allButLast);

  FrameInput bounce = inputFor(keys, -40.0, fixture);
  bounce.userScrolled = true;
  bounce.nonAnchorKeys = allButLast;
  Virtualizer::update(container, bounce);
  CHECK(!container.state.invertedBottomReleased);

  // The reply then grows past the screen, and its new bottom must be followed.
  for (int flush = 1; flush <= 6; ++flush) {
    Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 300.0});
    FrameInput rest = inputFor(keys, container.revision.offsetY, fixture);
    rest.nonAnchorKeys = allButLast;
    Virtualizer::update(container, rest);
  }
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);
}

/*
 * A reader scrolled up into a reply, which then shrinks until its bottom reaches them, like a
 * code block closing. The reader did not go back to the bottom. The list must keep not
 * following, or the next token pulls them to the end of the reply they were reading.
 */
TEST(inverted_bottom_pin_stays_released_when_the_reply_shrinks_onto_the_reader) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = 3000.0;
  measureRows(container, heights);
  // Height of everything above the reply. The shrink stops exactly at the reader.
  double headRows = 100.0 * static_cast<double>(keys.size() - 1);

  double bottom = settleAtBottom(container, keys, fixture);
  double parked = bottom - 400.0;
  FrameInput drag = inputFor(keys, parked, fixture);
  drag.userScrolled = true;
  Virtualizer::update(container, drag);
  CHECK(container.state.invertedBottomReleased);

  // Shrink the last row until the bottom lands exactly on the reader's offset.
  double shrunk = parked + WINDOW_HEIGHT - headRows;
  Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, shrunk});
  Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  CHECK(container.state.invertedBottomReleased);

  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, shrunk + flush * 150.0});
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK_NEAR(container.revision.offsetY, parked, 1.0);
}

/*
 * Regenerating a reply the reader scrolled past. The reply empties, the bottom rises above the
 * reader, the host clamps the offset to it as a user scroll, then the core's correction moves
 * a few points toward the bottom. That move is the core's own write coming back, not the reader
 * returning. The list must keep not following while the reply streams back in.
 */
TEST(inverted_bottom_pin_stays_released_when_a_correction_echo_nears_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = 3000.0;
  measureRows(container, heights);

  double bottom = settleAtBottom(container, keys, fixture);
  double parked = bottom - 400.0;
  FrameInput drag = inputFor(keys, parked, fixture);
  drag.userScrolled = true;
  Virtualizer::update(container, drag);
  CHECK(container.state.invertedBottomReleased);

  /*
   * The reply loses 1400 points. The new bottom is 1000 points above the reader. Computed
   * here because the stored total only updates on the next frame.
   */
  const double emptied = 1600.0;
  Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, emptied});
  double shrunkBottom = bottom - (3000.0 - emptied);
  CHECK(shrunkBottom < parked);

  // The host clamps just short of the new bottom, then reports the core's nudge onto it.
  FrameInput clamp = inputFor(keys, shrunkBottom - 20.0, fixture);
  clamp.userScrolled = true;
  Virtualizer::update(container, clamp);
  FrameInput echo = inputFor(keys, shrunkBottom, fixture);
  Virtualizer::update(container, echo);
  CHECK(container.state.invertedBottomReleased);

  // The reply streams back in and the reader stays where the clamp left them.
  double rested = container.revision.offsetY;
  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, emptied + flush * 150.0});
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK_NEAR(container.revision.offsetY, rested, 1.0);
}

/*
 * Edge callbacks follow the data order in every layout. Inverted rests at the end but does not
 * swap the edges. An inverted chat at its bottom is at the end of the data. onStartReached
 * must not fire there, or each load of older rows would fire it again every frame.
 */
TEST(inverted_list_reports_end_at_the_bottom_and_start_at_the_top) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  int startReached = 0;
  int endReached = 0;
  container.onStartReachedCallback = [&]() { startReached++; };
  container.onEndReachedCallback = [&]() { endReached++; };

  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);
  CHECK_NEAR(bottom, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);

  int startAtBottom = startReached;
  CHECK(endReached >= 1);

  // Rows prepended while resting at the bottom do not fire the start edge.
  for (int round = 0; round < 3; ++round) {
    std::vector<std::string> grown = keysFor(6, "older" + std::to_string(round) + "_");
    grown.insert(grown.end(), keys.begin(), keys.end());
    keys = grown;
    FrameInput input = inputFor(keys, container.revision.offsetY, fixture);
    Virtualizer::update(container, input);
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK_EQ(startReached, startAtBottom);

  // Scrolling to the top fires the start edge.
  FrameInput top = inputFor(keys, 0.0, fixture);
  top.userScrolled = true;
  Virtualizer::update(container, top);
  CHECK(startReached > startAtBottom);
}

/*
 * A chat a little taller than the viewport has both edge zones overlap. Scrolled to its top it
 * still fires onStartReached, or its older rows could never load. A chat shorter than the
 * viewport reports only the end.
 */
TEST(short_scrollable_inverted_chat_fires_start_at_its_top) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(12);
  Container container;
  int startReached = 0;
  int endReached = 0;
  container.onStartReachedCallback = [&]() { startReached++; };
  container.onEndReachedCallback = [&]() { endReached++; };

  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  settleAtBottom(container, keys, fixture);
  CHECK(endReached >= 1);

  FrameInput top = inputFor(keys, 0.0, fixture);
  top.userScrolled = true;
  Virtualizer::update(container, top);
  Virtualizer::update(container, top);
  CHECK(startReached >= 1);

  std::vector<std::string> fewKeys = keysFor(3, "few");
  Container shortContainer;
  int shortStart = 0;
  int shortEnd = 0;
  shortContainer.onStartReachedCallback = [&]() { shortStart++; };
  shortContainer.onEndReachedCallback = [&]() { shortEnd++; };
  Virtualizer::update(shortContainer, inputFor(fewKeys, 0.0, fixture));
  measureRows(shortContainer, std::vector<double>(fewKeys.size(), 100.0));
  settleAtBottom(shortContainer, fewKeys, fixture);
  CHECK(shortEnd >= 1);
  CHECK_EQ(shortStart, 0);
}

/*
 * A slow drag away from the bottom starts inside INVERTED_FOLLOW_BAND, where the list still
 * follows the bottom. While the finger is down, including commits between touch frames, the
 * list must not pull the view back. Lifting inside the band hands the view back to the bottom.
 */
TEST(inverted_bottom_pin_yields_while_a_finger_is_down_inside_the_band) {
  Fixture fixture;
  fixture.inverted = true;

  // Like the chat, only the newest row can be an anchor.
  std::vector<std::string> keys = keysFor(20);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture, allButLast);

  double offset = bottom;
  for (int frame = 0; frame < 8; ++frame) {
    offset -= 2.0;
    FrameInput drag = inputFor(keys, offset, fixture);
    drag.nonAnchorKeys = allButLast;
    drag.userScrolled = true;
    drag.scrollPhase = ScrollPhase::Dragging;
    Virtualizer::update(container, drag);
    CHECK_NEAR(container.revision.offsetY, offset, 0.01);

    // A commit between two touch frames reports the same offset with the finger still down.
    FrameInput commit = inputFor(keys, offset, fixture);
    commit.nonAnchorKeys = allButLast;
    commit.userScrolled = true;
    commit.scrollPhase = ScrollPhase::Dragging;
    Virtualizer::update(container, commit);
    CHECK_NEAR(container.revision.offsetY, offset, 0.01);
  }
  CHECK(!container.state.invertedBottomReleased);

  // The finger lifts 16 points above the bottom, inside the band. The view goes back to the bottom.
  FrameInput lift = inputFor(keys, offset, fixture);
  lift.nonAnchorKeys = allButLast;
  Virtualizer::update(container, lift);
  for (int frame = 0; frame < 4; ++frame) {
    FrameInput rest = inputFor(keys, container.revision.offsetY, fixture);
    rest.nonAnchorKeys = allButLast;
    Virtualizer::update(container, rest);
  }
  CHECK_NEAR(container.revision.offsetY, bottom, 1.0);
}

/*
 * Growth measured between frames is followed right away. The reply's footer mounts when the
 * stream ends and is measured after the last commit. With no frame after that, the measuring
 * step itself must move the view to the new bottom, or the last row stays cut off.
 */
TEST(inverted_bottom_pin_follows_growth_measured_between_frames) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture, allButLast);
  CHECK_NEAR(bottom, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);

  // The newest row grows by 40 points with no frame, and the view is already at the new bottom.
  Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 140.0});
  CHECK_NEAR(container.revision.offsetY, bottom + 40.0, 0.01);
  CHECK(container.state.offsetCorrected);

  // A reader who scrolled away is left alone.
  FrameInput away = inputFor(keys, bottom - 300.0, fixture);
  away.nonAnchorKeys = allButLast;
  away.userScrolled = true;
  Virtualizer::update(container, away);
  CHECK(container.state.invertedBottomReleased);
  double parked = container.revision.offsetY;
  Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 200.0});
  CHECK_NEAR(container.revision.offsetY, parked, 0.01);
}

/*
 * Settle an inverted list on 19 rows of 100 and a 1 point end marker, then insert a reply
 * before the marker the way a chat adds one. Returns the reply's index.
 */
static std::size_t settleWithReply(Container& container, std::vector<std::string>& keys, const Fixture& fixture) {
  keys = keysFor(20);
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = 1.0;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, heights);
  settleAtBottom(container, keys, fixture);
  keys.insert(keys.end() - 1, "reply");
  std::size_t reply = keys.size() - 2;
  double offset = container.revision.offsetY;
  for (int frame = 0; frame < 6; ++frame) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    if (frame == 0) {
      Virtualizer::updateRowAtIndex(container, reply, {WINDOW_WIDTH, 100.0});
    }
    offset = container.revision.offsetY;
  }
  return reply;
}

/*
 * With followAppends, a row inserted at the bottom and a reply growing in place keep the
 * bottom in view, even when older rows fill the top of the screen and hold the anchor.
 */
TEST(inverted_list_with_follow_appends_follows_a_row_growing_in_place) {
  Fixture fixture;
  fixture.inverted = true;
  fixture.followAppends = true;

  Container container;
  std::vector<std::string> keys;
  std::size_t reply = settleWithReply(container, keys, fixture);
  double bottom = container.revision.contentHeight - WINDOW_HEIGHT;
  CHECK_NEAR(container.revision.offsetY, bottom, 1.0);
  CHECK(!container.state.invertedOpeningPin);
  CHECK(container.state.anchor.key != keys[reply]);

  // The reply grows between frames, then again with frames after it.
  Virtualizer::updateRowAtIndex(container, reply, {WINDOW_WIDTH, 160.0});
  CHECK_NEAR(container.revision.offsetY, bottom + 60.0, 0.01);
  Virtualizer::updateRowAtIndex(container, reply, {WINDOW_WIDTH, 240.0});
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK_NEAR(container.revision.offsetY, bottom + 140.0, 1.0);

  // A reader who scrolled away is left alone.
  FrameInput away = inputFor(keys, container.revision.offsetY - 300.0, fixture);
  away.userScrolled = true;
  Virtualizer::update(container, away);
  CHECK(container.state.invertedBottomReleased);
  double parked = container.revision.offsetY;
  Virtualizer::updateRowAtIndex(container, reply, {WINDOW_WIDTH, 320.0});
  CHECK_NEAR(container.revision.offsetY, parked, 0.01);
}

/*
 * Without followAppends the same growth keeps the rows on screen still, as before.
 */
TEST(inverted_list_without_follow_appends_holds_its_anchor_while_a_row_grows) {
  Fixture fixture;
  fixture.inverted = true;

  Container container;
  std::vector<std::string> keys;
  std::size_t reply = settleWithReply(container, keys, fixture);
  double resting = container.revision.offsetY;

  Virtualizer::updateRowAtIndex(container, reply, {WINDOW_WIDTH, 160.0});
  CHECK_NEAR(container.revision.offsetY, resting, 0.01);
}

/*
 * The at end callback fires when the user scrolls an inverted list away from its bottom and
 * again when they scroll back. Content growth alone never fires it.
 */
TEST(inverted_list_reports_leaving_and_returning_to_its_end) {
  Fixture fixture;
  fixture.inverted = true;
  fixture.followAppends = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  std::vector<bool> reports;
  container.onAtEndChangeCallback = [&reports](bool atEnd) { reports.push_back(atEnd); };
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);
  Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 300.0});
  Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  CHECK(reports.empty());

  FrameInput away = inputFor(keys, bottom - 300.0, fixture);
  away.userScrolled = true;
  Virtualizer::update(container, away);
  CHECK_EQ(reports.size(), std::size_t(1));
  CHECK(!reports.back());

  FrameInput back = inputFor(keys, container.revision.contentHeight - WINDOW_HEIGHT, fixture);
  back.userScrolled = true;
  Virtualizer::update(container, back);
  CHECK_EQ(reports.size(), std::size_t(2));
  CHECK(reports.back());
}

/*
 * The newest message of a chat is taller than the screen and the app scrolls to its top, like a
 * jump to the start of a long reply. The list was never dragged and still counts as resting on
 * its bottom. The command leaves the bottom the way a drag does and reports it. Otherwise the
 * next frame would pull the view from the top of the row down to its end.
 */
TEST(inverted_scroll_to_the_top_of_a_tall_last_row_stays_there) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  Container container;
  std::vector<bool> reports;
  container.onAtEndChangeCallback = [&reports](bool atEnd) { reports.push_back(atEnd); };
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  std::vector<double> heights(keys.size(), 100.0);
  heights.back() = WINDOW_HEIGHT + 100.0;
  measureRows(container, heights);
  double bottom = settleAtBottom(container, keys, fixture);
  CHECK_NEAR(bottom, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);
  CHECK(!container.state.invertedBottomReleased);

  container.scrollToRow(keys.size() - 1, 0.0);
  Virtualizer::update(container, inputFor(keys, bottom, fixture));
  CHECK(container.state.operation.has_value());
  double target = container.revision.offsetY;
  CHECK_NEAR(target, offsetOf(container, keys.size() - 1), 1.0);
  CHECK_NEAR(target, bottom - 100.0, 1.0);

  // The host applies the write and reports it back, then rests.
  FrameInput echo = inputFor(keys, target, fixture);
  echo.commitToken = container.state.operation->commitToken;
  Virtualizer::update(container, echo);
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK(!container.state.operation.has_value());
  CHECK_NEAR(container.revision.offsetY, target, 1.0);
  CHECK(container.state.invertedBottomReleased);
  CHECK_EQ(reports.size(), std::size_t(1));
  CHECK(!reports.back());

  // The row keeps growing, like a streaming reply, and the reader stays at its top.
  Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, WINDOW_HEIGHT + 400.0});
  Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  CHECK_NEAR(container.revision.offsetY, target, 1.0);
}

/*
 * A command that lands on the bottom does not leave it. Growth of the newest row is still followed.
 */
TEST(inverted_scroll_to_a_short_last_row_keeps_following_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(20);
  std::vector<std::string> allButLast(keys.begin(), keys.end() - 1);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture, allButLast);

  auto frameAt = [&](double offset) {
    FrameInput input = inputFor(keys, offset, fixture);
    input.nonAnchorKeys = allButLast;
    return input;
  };
  container.scrollToRow(keys.size() - 1, 0.0);
  Virtualizer::update(container, frameAt(bottom));
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, frameAt(container.revision.offsetY));
  }
  CHECK_NEAR(container.revision.offsetY, bottom, 1.0);
  CHECK(!container.state.invertedBottomReleased);

  Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 400.0});
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, frameAt(container.revision.offsetY));
  }
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 1.0);
}

namespace {

/*
 * A chat with followAppends rests at its bottom. A jump to an older row takes it away, then the
 * host animates back to the bottom on its own with plain reports. Returns the bottom.
 */
double jumpAwayThenAnimateBackToTheBottom(
  Container& container,
  const std::vector<std::string>& keys,
  const Fixture& fixture) {
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));
  double bottom = settleAtBottom(container, keys, fixture);

  container.scrollToRow(10, 0.0);
  Virtualizer::update(container, inputFor(keys, bottom, fixture));
  for (int frame = 0; frame < 4; ++frame) {
    FrameInput echo = inputFor(keys, container.revision.offsetY, fixture);
    echo.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, echo);
  }
  CHECK_NEAR(container.revision.offsetY, 1000.0, 1.0);
  CHECK(container.state.invertedBottomReleased);

  for (double offset = 1400.0; offset < bottom; offset += 400.0) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
  }
  Virtualizer::update(container, inputFor(keys, bottom, fixture));
  CHECK(container.state.invertedBottomReleased);
  return bottom;
}

}

/*
 * Jump to a quoted message, then jump to the latest with an animation. The host animates to the
 * bottom itself and lands the command with the core once it is there. The landing moves nothing.
 * It still puts the list back on its bottom: the end is reported and the next message is followed.
 */
TEST(inverted_scroll_to_end_that_lands_without_moving_returns_to_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;
  fixture.followAppends = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  std::vector<bool> reports;
  container.onAtEndChangeCallback = [&reports](bool atEnd) { reports.push_back(atEnd); };
  double bottom = jumpAwayThenAnimateBackToTheBottom(container, keys, fixture);
  CHECK_EQ(reports.size(), std::size_t(1));
  CHECK(!reports.back());

  container.scrollToEnd();
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK_NEAR(container.revision.offsetY, bottom, 1.0);
  CHECK(!container.state.pendingScrollToEnd);
  CHECK(!container.state.invertedBottomReleased);
  CHECK_EQ(reports.size(), std::size_t(2));
  CHECK(reports.back());

  keys.push_back("incoming");
  Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  Virtualizer::updateRowAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 100.0});
  for (int frame = 0; frame < 6; ++frame) {
    FrameInput echo = inputFor(keys, container.revision.offsetY, fixture);
    echo.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, echo);
  }
  CHECK_NEAR(container.revision.offsetY, bottom + 100.0, 1.0);
}

/*
 * The same landing by a scroll to the newest row.
 */
TEST(inverted_scroll_to_the_last_row_that_lands_without_moving_returns_to_the_bottom) {
  Fixture fixture;
  fixture.inverted = true;
  fixture.followAppends = true;

  std::vector<std::string> keys = keysFor(40);
  Container container;
  double bottom = jumpAwayThenAnimateBackToTheBottom(container, keys, fixture);

  container.scrollToRow(keys.size() - 1, 1.0);
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK_NEAR(container.revision.offsetY, bottom, 1.0);
  CHECK(!container.state.operation.has_value());
  CHECK(!container.state.invertedBottomReleased);
}

/*
 * A scroll to an index in a grid lands on its row's track. Rows measured afterwards in another
 * track must not pull the landed row away, even though the capture picked a row in that track.
 */
TEST(grid_scroll_to_index_keeps_the_landed_row_while_other_tracks_get_measured) {
  Fixture fixture;
  fixture.numberOfColumns = 2;

  std::vector<std::string> keys = keysFor(120);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  layoutRows(container, 0, keys.size() - 1, [](std::size_t) { return Size{WINDOW_WIDTH / 2.0, 100.0}; });
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  // Row 41 sits in the second track, level with row 40 in the first.
  container.scrollToRow(41, 0.0);
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK(container.state.operation.has_value());
  double target = container.revision.offsetY;
  CHECK_NEAR(target, offsetOf(container, 41), 0.001);
  FrameInput echo = inputFor(keys, target, fixture);
  echo.commitToken = container.state.operation->commitToken;
  Virtualizer::update(container, echo);
  CHECK(!container.state.operation.has_value());

  // The layout pass of the landing frame measures a row above in the first track taller.
  Virtualizer::applyRowSize(container, 38, {WINDOW_WIDTH / 2.0, 300.0});
  Virtualizer::commitRowSizes(container, 38);
  Virtualizer::recomputeContentSize(container);
  CHECK_NEAR(offsetOf(container, 41) - container.revision.offsetY, 0.0, 0.5);

  for (int frame = 0; frame < 3; ++frame) {
    FrameInput report = inputFor(keys, container.revision.offsetY, fixture);
    Virtualizer::update(container, report);
    CHECK_NEAR(offsetOf(container, 41) - container.revision.offsetY, 0.0, 0.5);
  }
}

/*
 * A grid's tracks move on their own as rows above the screen get measured. The anchor must
 * stay on the row it already holds while that row is on screen, or each frame holds a
 * different track and the row the reader was on drifts.
 */
TEST(grid_keeps_its_anchor_row_while_other_tracks_get_measured) {
  Fixture fixture;
  fixture.numberOfColumns = 3;

  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  // Every row is 100 tall. The tracks line up and rows 30, 31 and 32 start at 1000.
  for (std::size_t index = 0; index < keys.size(); ++index) {
    Virtualizer::applyRowSize(container, index, {WINDOW_WIDTH / 3.0, 100.0});
  }
  Virtualizer::commitRowSizes(container, 0);
  Virtualizer::recomputeContentSize(container);
  Virtualizer::update(container, inputFor(keys, 1040.0, fixture));
  Virtualizer::update(container, inputFor(keys, 1040.0, fixture));
  CHECK_EQ(container.state.anchor.key, std::string("k30"));
  double k30Screen = offsetOf(container, 30) - container.revision.offsetY;

  // Prepend a full row of tracks, then measure the new rows unevenly: track 0 grows, track 2 shrinks.
  std::vector<std::string> next = keysFor(3, "n");
  next.insert(next.end(), keys.begin(), keys.end());
  Virtualizer::update(container, inputFor(next, 1040.0, fixture));
  CHECK_NEAR(offsetOf(container, 33) - container.revision.offsetY, k30Screen, 0.5);
  Virtualizer::applyRowSize(container, 0, {WINDOW_WIDTH / 3.0, 300.0});
  Virtualizer::applyRowSize(container, 1, {WINDOW_WIDTH / 3.0, 100.0});
  Virtualizer::applyRowSize(container, 2, {WINDOW_WIDTH / 3.0, 20.0});
  Virtualizer::commitRowSizes(container, 0);
  Virtualizer::recomputeContentSize(container);
  CHECK_NEAR(offsetOf(container, 33) - container.revision.offsetY, k30Screen, 0.5);

  // Frames go by with the host at the corrected offset. k30 keeps holding, not the row in track 2.
  for (int frame = 0; frame < 4; ++frame) {
    FrameInput own = inputFor(next, container.revision.offsetY, fixture);
    own.offsetEnabled = container.state.offsetCorrected;
    own.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, own);
    FrameInput echo = inputFor(next, container.revision.offsetY, fixture);
    echo.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, echo);
    CHECK_EQ(container.state.anchor.key, std::string("k30"));
    CHECK_NEAR(offsetOf(container, 33) - container.revision.offsetY, k30Screen, 0.5);
  }
}

/*
 * A scroll to the end of a masonry grid lands while rows above the viewport are still being
 * measured. The host reports the end, and that frame's layout pass measures one more row above
 * the viewport, in the track of the row at the top of the screen. Only that track moves, and the
 * other track still ends where it did. The command is still running and keeps the view on the
 * end. Holding the top row's track would leave the view short of it.
 */
TEST(grid_scroll_to_end_stays_on_the_end_while_rows_above_get_measured) {
  Fixture fixture;
  fixture.numberOfColumns = 2;

  std::vector<std::string> keys = keysFor(120);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  layoutRows(container, 0, keys.size() - 1, [](std::size_t) { return Size{WINDOW_WIDTH / 2.0, 100.0}; });
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  auto measure = [&](std::size_t index, double height) {
    layoutRows(container, index, index, [&](std::size_t) { return Size{WINDOW_WIDTH / 2.0, height}; });
  };
  auto endOffset = [&]() { return container.revision.contentHeight - WINDOW_HEIGHT; };

  container.scrollToEnd();
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK(container.state.operation.has_value());
  CHECK_NEAR(container.revision.offsetY, endOffset(), 0.5);

  /*
   * The landing frame's layout pass measures a row above the viewport in the second track a
   * little shorter. The first track stays the longer one and holds the row at the top of the screen.
   */
  measure(91, 80.0);
  CHECK_NEAR(container.revision.offsetY, endOffset(), 0.5);

  // The host reports the end. The pass after it measures a row of the first track much shorter.
  FrameInput echo = inputFor(keys, container.revision.offsetY, fixture);
  echo.commitToken = container.state.operation->commitToken;
  Virtualizer::update(container, echo);
  measure(88, 40.0);
  CHECK_NEAR(container.revision.offsetY, endOffset(), 0.5);

  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, inputFor(keys, container.revision.offsetY, fixture));
  }
  CHECK_NEAR(container.revision.offsetY, endOffset(), 0.5);
  CHECK(!container.state.operation.has_value());
  CHECK(!container.state.pendingScrollToEnd);
}

/*
 * A scroll to the end of a grid has landed, and a row on screen keeps changing size on every
 * frame, like a cell that animates its height. The command waits a few frames for the rows to
 * settle and then ends. It does not stay alive and write the offset for as long as the row moves.
 */
TEST(grid_scroll_to_end_ends_while_a_row_keeps_resizing) {
  Fixture fixture;
  fixture.numberOfColumns = 2;

  std::vector<std::string> keys = keysFor(120);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  layoutRows(container, 0, keys.size() - 1, [](std::size_t) { return Size{WINDOW_WIDTH / 2.0, 100.0}; });
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  container.scrollToEnd();
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK(container.state.operation.has_value());

  // Row 117 is in the second track, which stays the shorter one. The end never moves.
  int framesWithOperation = 0;
  int framesWithWrite = 0;
  for (int frame = 0; frame < 40; ++frame) {
    double height = frame % 2 == 0 ? 60.0 : 70.0;
    layoutRows(container, 117, 117, [&](std::size_t) { return Size{WINDOW_WIDTH / 2.0, height}; });
    FrameInput echo = inputFor(keys, container.revision.offsetY, fixture);
    echo.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, echo);
    framesWithOperation += container.state.operation ? 1 : 0;
    framesWithWrite += container.state.offsetCorrected ? 1 : 0;
  }
  CHECK(framesWithOperation <= 4);
  CHECK(framesWithWrite <= 4);
  CHECK(!container.state.operation.has_value());
  CHECK(!container.state.pendingScrollToEnd);
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 0.5);
}

namespace {

/*
 * The size of a row in a two column grid whose first track holds tall rows and whose second
 * track holds short ones.
 */
Size unevenTrackSize(std::size_t index) {
  return {WINDOW_WIDTH / 2.0, index % 2 == 0 ? 400.0 : 50.0};
}

/*
 * A layout pass of that grid: sizes for the rows of the measured range in one batch.
 */
void measureUnevenTracks(Container& container) {
  IndexRange measured = container.getMeasuredRange();
  if (measured.low != UNDEFINED_INDEX) {
    layoutRows(container, measured.low, measured.high, unevenTrackSize);
  }
}

}

/*
 * A masonry grid whose tracks end far apart gets a row appended to the short track. That row
 * is the last one by index, and it sits far above the end of the content, outside the measured
 * range. It never gets a real size. A scroll to the end is done once the row that ends the
 * content has one.
 */
TEST(grid_scroll_to_end_finishes_when_the_last_row_sits_on_a_short_track) {
  Fixture fixture;
  fixture.numberOfColumns = 2;

  std::vector<std::string> keys = keysFor(41);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  layoutRows(container, 0, keys.size() - 1, unevenTrackSize);
  keys.push_back("appended");
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  container.scrollToEnd();
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  for (int frame = 0; frame < 8; ++frame) {
    measureUnevenTracks(container);
    FrameInput echo = inputFor(keys, container.revision.offsetY, fixture);
    echo.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, echo);
  }
  CHECK(!container.revision.rows.back().measured);
  CHECK(offsetOf(container, keys.size() - 1) + sizeOf(container, keys.size() - 1) < container.revision.offsetY);
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 0.5);
  CHECK(!container.state.pendingScrollToEnd);
  CHECK(!container.state.operation.has_value());
}

/*
 * The same grid inverted, still opening on its bottom when the row is appended. The opening
 * pin is done once the row that ends the content has a real size.
 */
TEST(inverted_grid_finishes_opening_when_the_last_row_sits_on_a_short_track) {
  Fixture fixture;
  fixture.numberOfColumns = 2;
  fixture.inverted = true;

  std::vector<std::string> keys = keysFor(41);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK(!container.state.invertedInitialized);
  layoutRows(container, 0, keys.size() - 1, unevenTrackSize);

  keys.push_back("appended");
  for (int frame = 0; frame < 8; ++frame) {
    FrameInput echo = inputFor(keys, container.revision.offsetY, fixture);
    echo.commitToken = container.state.operation ? container.state.operation->commitToken : 0;
    Virtualizer::update(container, echo);
    measureUnevenTracks(container);
  }
  CHECK(!container.revision.rows.back().measured);
  CHECK(offsetOf(container, keys.size() - 1) + sizeOf(container, keys.size() - 1) < container.revision.offsetY);
  CHECK_NEAR(container.revision.offsetY, container.revision.contentHeight - WINDOW_HEIGHT, 0.5);
  CHECK(container.state.invertedInitialized);
  CHECK(!container.state.operation.has_value());
}

TEST(scroll_to_index_lands_on_the_requested_row) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  container.scrollToRow(321);

  double offset = 0.0;
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    offset = container.revision.offsetY;
  }

  std::size_t targetIndex = container.indexOfKey("k321");
  CHECK_NEAR(offset, offsetOf(container, targetIndex), 1.0);
  checkNoRowLost(container, "after scrollToRow");
}

TEST(scroll_to_index_issued_while_scroll_to_end_is_pending_wins) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  container.scrollToEnd();
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK(container.state.pendingScrollToEnd);
  container.scrollToRow(120);
  CHECK(!container.state.pendingScrollToEnd);

  double offset = container.revision.offsetY;
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    offset = container.revision.offsetY;
  }
  CHECK_NEAR(offset, offsetOf(container, container.indexOfKey("k120")), 1.0);

  container.scrollToEnd();
  CHECK_EQ(container.state.scrollToRowTarget, UNDEFINED_INDEX);
}

TEST(content_shrinking_below_the_offset_pulls_the_view_back) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(300);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  double deepOffset = 20000.0;
  Virtualizer::update(container, inputFor(keys, deepOffset, fixture));

  std::vector<std::string> collapsed(keys.begin(), keys.begin() + 20);
  double offset = deepOffset;
  for (int frame = 0; frame < 6; ++frame) {
    Virtualizer::update(container, inputFor(collapsed, offset, fixture));
    offset = container.revision.offsetY;
  }

  double maxOffset = std::max(0.0, container.revision.contentHeight - WINDOW_HEIGHT);
  CHECK(offset <= maxOffset + 1.0);
  checkNoRowLost(container, "after collapse");
}
