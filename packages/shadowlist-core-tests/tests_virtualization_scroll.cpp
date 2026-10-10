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
#include <cmath>
#include <cstdint>
#include <set>
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
  CHECK_NEAR(offsetOf(container, 80), container.revision.containerOffsetY, 1.0);

  std::vector<std::string> prepended = keysFor(30, "older");
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  Virtualizer::update(container, inputFor(prepended, offset, fixture));

  std::size_t movedIndex = container.findElementIndexByKey("k80");
  CHECK(movedIndex != UNDEFINED_INDEX);
  CHECK_EQ(movedIndex, static_cast<std::size_t>(110));
  // The same row must still be at the top of the screen.
  CHECK_NEAR(offsetOf(container, movedIndex), container.revision.containerOffsetY, 1.0);
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
    return fixture.horizontal ? container.revision.containerOffsetX : container.revision.containerOffsetY;
  };

  auto measureAll = [&](std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
      Size size = fixture.horizontal ? Size{100.0, WINDOW_HEIGHT} : Size{WINDOW_WIDTH, 100.0};
      Virtualizer::updateElementAtIndex(container, index, size);
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

  std::size_t movedIndex = container.findElementIndexByKey("k" + std::to_string(restIndex));
  CHECK_EQ(movedIndex, restIndex + 10);
  // The row stays where it was on screen, and the offset grew by the new rows.
  CHECK_NEAR(offsetOf(container, movedIndex) - currentOffset(), restKeyScreenPosition, 1.0);
  CHECK(container.containerOffsetCorrected);

  // The host applies the new offset and measures the new rows, and nothing moves.
  double corrected = currentOffset();
  FrameInput confirm = inputFor(prepended, corrected, fixture);
  confirm.headerSize = headerSize;
  confirm.containerOffsetEnabled = true;
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
    input.containerOffsetEnabled = false;
    Virtualizer::update(container, input);
    offset = container.revision.containerOffsetY;
  }

  double maxOffset = container.revision.totalContainerHeight - WINDOW_HEIGHT;
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
    offset = container.revision.containerOffsetY;
  }

  FrameInput jump = inputFor(keys, 0.0, fixture);
  jump.userScrolled = true;
  jump.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(container, jump);

  auto window = reportedWindow(container);
  CHECK(window.first != UNDEFINED_INDEX);
  CHECK_EQ(window.first, static_cast<std::size_t>(0));
  checkNoRowLost(container, "inverted after jump to top");

  // The jump must stick and not be pulled back to the bottom.
  FrameInput settle = inputFor(keys, 0.0, fixture);
  Virtualizer::update(container, settle);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 1.0);
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
  CHECK_NEAR(container.revision.containerOffsetY, newBottom, 1.0);
  CHECK(!container.invertedBottomReleased);

  // A partial swap that keeps a visible row still holds that row, like any other change.
  FrameInput up = inputFor(other, newBottom - 300.0, fixture);
  up.userScrolled = true;
  Virtualizer::update(container, up);
  Virtualizer::update(container, inputFor(other, newBottom - 300.0, fixture));
  std::string held = container.anchor.key;
  double heldScreen = offsetOf(container, container.findElementIndexByKey(held)) - container.revision.containerOffsetY;
  std::vector<std::string> mixed = keysFor(10, "c");
  mixed.push_back(held);
  std::vector<std::string> after = keysFor(10, "d");
  mixed.insert(mixed.end(), after.begin(), after.end());
  Virtualizer::update(container, inputFor(mixed, container.revision.containerOffsetY, fixture));
  CHECK_NEAR(offsetOf(container, container.findElementIndexByKey(held)) - container.revision.containerOffsetY, heldScreen, 1.0);
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
  CHECK(container.operation && container.operation->type == OperationType::ScrollToEnd);
  double target = container.revision.containerOffsetY;
  CHECK_NEAR(target, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  // Our write comes back before the host confirms it, and the pass measures rows above the view.
  FrameInput ownWrite = inputFor(keys, target, fixture);
  ownWrite.containerOffsetEnabled = true;
  ownWrite.commitToken = container.operation->id;
  Virtualizer::update(container, ownWrite);
  CHECK(container.operation);
  Virtualizer::updateElementAtIndex(container, 30, {WINDOW_WIDTH, 160.0});
  Virtualizer::updateElementAtIndex(container, 40, {WINDOW_WIDTH, 130.0});
  Virtualizer::recomputeTotalSize(container);
  CHECK_NEAR(container.revision.containerOffsetY, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
  CHECK(container.containerOffsetCorrected);
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
  CHECK_NEAR(bottom, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  // The finger drags 400 points up, still inside the tall last row.
  double dragged = bottom - 400.0;
  FrameInput drag = inputFor(keys, dragged, fixture);
  drag.userScrolled = true;
  Virtualizer::update(container, drag);
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);

  // The reply keeps streaming while the finger rests, and the view must not move.
  for (int flush = 1; flush <= 5; ++flush) {
    Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 3000.0 + flush * 60.0});
    Virtualizer::update(container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);
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
  drag.nonAnchorableKeys = allButLast;
  Virtualizer::update(container, drag);
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);

  for (int flush = 1; flush <= 5; ++flush) {
    Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 80.0});
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, dragged, 1.0);
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
  CHECK(container.invertedBottomReleased);

  FrameInput back = inputFor(keys, bottom, fixture);
  back.userScrolled = true;
  Virtualizer::update(container, back);
  CHECK(!container.invertedBottomReleased);

  Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 3300.0});
  double offset = container.revision.containerOffsetY;
  for (int frame = 0; frame < 4; ++frame) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    offset = container.revision.containerOffsetY;
  }
  CHECK_NEAR(offset, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
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
  nudge.nonAnchorableKeys = allButLast;
  Virtualizer::update(container, nudge);
  CHECK(!container.invertedBottomReleased);

  // Still following. The view moves with the growing last row.
  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 120.0});
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
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
  bounce.nonAnchorableKeys = allButLast;
  Virtualizer::update(container, bounce);
  CHECK(!container.invertedBottomReleased);

  // The reply then grows past the screen, and its new bottom must be followed.
  for (int flush = 1; flush <= 6; ++flush) {
    Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 100.0 + flush * 300.0});
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);
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
  CHECK(container.invertedBottomReleased);

  // Shrink the last row until the bottom lands exactly on the reader's offset.
  double shrunk = parked + WINDOW_HEIGHT - headRows;
  Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, shrunk});
  Virtualizer::update(container, inputFor(keys, container.revision.containerOffsetY, fixture));
  CHECK(container.invertedBottomReleased);

  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, shrunk + flush * 150.0});
    Virtualizer::update(container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_NEAR(container.revision.containerOffsetY, parked, 1.0);
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
  CHECK(container.invertedBottomReleased);

  /*
   * The reply loses 1400 points. The new bottom is 1000 points above the reader. Computed
   * here because the stored total only updates on the next frame.
   */
  const double emptied = 1600.0;
  Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, emptied});
  double shrunkBottom = bottom - (3000.0 - emptied);
  CHECK(shrunkBottom < parked);

  // The host clamps just short of the new bottom, then reports the core's nudge onto it.
  FrameInput clamp = inputFor(keys, shrunkBottom - 20.0, fixture);
  clamp.userScrolled = true;
  Virtualizer::update(container, clamp);
  FrameInput echo = inputFor(keys, shrunkBottom, fixture);
  Virtualizer::update(container, echo);
  CHECK(container.invertedBottomReleased);

  // The reply streams back in and the reader stays where the clamp left them.
  double rested = container.revision.containerOffsetY;
  for (int flush = 1; flush <= 4; ++flush) {
    Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, emptied + flush * 150.0});
    Virtualizer::update(container, inputFor(keys, container.revision.containerOffsetY, fixture));
  }
  CHECK_NEAR(container.revision.containerOffsetY, rested, 1.0);
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
  CHECK_NEAR(bottom, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  int startAtBottom = startReached;
  CHECK(endReached >= 1);

  // Rows prepended while resting at the bottom do not fire the start edge.
  for (int round = 0; round < 3; ++round) {
    std::vector<std::string> grown = keysFor(6, "older" + std::to_string(round) + "_");
    grown.insert(grown.end(), keys.begin(), keys.end());
    keys = grown;
    FrameInput input = inputFor(keys, container.revision.containerOffsetY, fixture);
    Virtualizer::update(container, input);
    Virtualizer::update(container, inputFor(keys, container.revision.containerOffsetY, fixture));
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
    drag.nonAnchorableKeys = allButLast;
    drag.userScrolled = true;
    drag.scrollPhase = ScrollPhase::Dragging;
    Virtualizer::update(container, drag);
    CHECK_NEAR(container.revision.containerOffsetY, offset, 0.01);

    // A commit between two touch frames reports the same offset with the finger still down.
    FrameInput commit = inputFor(keys, offset, fixture);
    commit.nonAnchorableKeys = allButLast;
    commit.userScrolled = true;
    commit.scrollPhase = ScrollPhase::Dragging;
    Virtualizer::update(container, commit);
    CHECK_NEAR(container.revision.containerOffsetY, offset, 0.01);
  }
  CHECK(!container.invertedBottomReleased);

  // The finger lifts 16 points above the bottom, inside the band. The view goes back to the bottom.
  FrameInput lift = inputFor(keys, offset, fixture);
  lift.nonAnchorableKeys = allButLast;
  Virtualizer::update(container, lift);
  for (int frame = 0; frame < 4; ++frame) {
    FrameInput rest = inputFor(keys, container.revision.containerOffsetY, fixture);
    rest.nonAnchorableKeys = allButLast;
    Virtualizer::update(container, rest);
  }
  CHECK_NEAR(container.revision.containerOffsetY, bottom, 1.0);
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
  CHECK_NEAR(bottom, container.revision.totalContainerHeight - WINDOW_HEIGHT, 1.0);

  // The newest row grows by 40 points with no frame, and the view is already at the new bottom.
  Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 140.0});
  CHECK_NEAR(container.revision.containerOffsetY, bottom + 40.0, 0.01);
  CHECK(container.containerOffsetCorrected);

  // A reader who scrolled away is left alone.
  FrameInput away = inputFor(keys, bottom - 300.0, fixture);
  away.nonAnchorableKeys = allButLast;
  away.userScrolled = true;
  Virtualizer::update(container, away);
  CHECK(container.invertedBottomReleased);
  double parked = container.revision.containerOffsetY;
  Virtualizer::updateElementAtIndex(container, keys.size() - 1, {WINDOW_WIDTH, 200.0});
  CHECK_NEAR(container.revision.containerOffsetY, parked, 0.01);
}

/*
 * A scroll to an index in a grid lands on its row's track. Rows measured afterwards in another
 * track must not pull the landed row away, even though the capture picked a row in that track.
 */
TEST(grid_scroll_to_index_keeps_the_landed_row_while_other_tracks_get_measured) {
  Fixture fixture;
  fixture.columns = 2;

  std::vector<std::string> keys = keysFor(120);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  for (std::size_t index = 0; index < keys.size(); ++index) {
    Virtualizer::applyElementSize(container, index, {WINDOW_WIDTH / 2.0, 100.0});
  }
  Virtualizer::commitElementSizes(container, 0);
  Virtualizer::recomputeTotalSize(container);
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));

  // Row 41 sits in the second track, level with row 40 in the first.
  container.scrollToIndex(41, 0.0);
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  CHECK(container.operation.has_value());
  double target = container.revision.containerOffsetY;
  CHECK_NEAR(target, offsetOf(container, 41), 0.001);
  FrameInput echo = inputFor(keys, target, fixture);
  echo.commitToken = container.operation->id;
  Virtualizer::update(container, echo);
  CHECK(!container.operation.has_value());

  // The layout pass of the landing frame measures a row above in the first track taller.
  Virtualizer::applyElementSize(container, 38, {WINDOW_WIDTH / 2.0, 300.0});
  Virtualizer::commitElementSizes(container, 38);
  Virtualizer::recomputeTotalSize(container);
  CHECK_NEAR(offsetOf(container, 41) - container.revision.containerOffsetY, 0.0, 0.5);

  for (int frame = 0; frame < 3; ++frame) {
    FrameInput report = inputFor(keys, container.revision.containerOffsetY, fixture);
    Virtualizer::update(container, report);
    CHECK_NEAR(offsetOf(container, 41) - container.revision.containerOffsetY, 0.0, 0.5);
  }
}

/*
 * A grid's tracks move on their own as rows above the screen get measured. The anchor must
 * stay on the row it already holds while that row is on screen, or each frame holds a
 * different track and the row the reader was on drifts.
 */
TEST(grid_keeps_its_anchor_row_while_other_tracks_get_measured) {
  Fixture fixture;
  fixture.columns = 3;

  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  // Every row is 100 tall. The tracks line up and rows 30, 31 and 32 start at 1000.
  for (std::size_t index = 0; index < keys.size(); ++index) {
    Virtualizer::applyElementSize(container, index, {WINDOW_WIDTH / 3.0, 100.0});
  }
  Virtualizer::commitElementSizes(container, 0);
  Virtualizer::recomputeTotalSize(container);
  Virtualizer::update(container, inputFor(keys, 1040.0, fixture));
  Virtualizer::update(container, inputFor(keys, 1040.0, fixture));
  CHECK_EQ(container.anchor.key, std::string("k30"));
  double k30Screen = offsetOf(container, 30) - container.revision.containerOffsetY;

  // Prepend a full row of tracks, then measure the new rows unevenly: track 0 grows, track 2 shrinks.
  std::vector<std::string> next = keysFor(3, "n");
  next.insert(next.end(), keys.begin(), keys.end());
  Virtualizer::update(container, inputFor(next, 1040.0, fixture));
  CHECK_NEAR(offsetOf(container, 33) - container.revision.containerOffsetY, k30Screen, 0.5);
  Virtualizer::applyElementSize(container, 0, {WINDOW_WIDTH / 3.0, 300.0});
  Virtualizer::applyElementSize(container, 1, {WINDOW_WIDTH / 3.0, 100.0});
  Virtualizer::applyElementSize(container, 2, {WINDOW_WIDTH / 3.0, 20.0});
  Virtualizer::commitElementSizes(container, 0);
  Virtualizer::recomputeTotalSize(container);
  CHECK_NEAR(offsetOf(container, 33) - container.revision.containerOffsetY, k30Screen, 0.5);

  // Frames go by with the host at the corrected offset. k30 keeps holding, not the row in track 2.
  for (int frame = 0; frame < 4; ++frame) {
    FrameInput own = inputFor(next, container.revision.containerOffsetY, fixture);
    own.containerOffsetEnabled = container.containerOffsetCorrected;
    own.commitToken = container.operation ? container.operation->id : 0;
    Virtualizer::update(container, own);
    FrameInput echo = inputFor(next, container.revision.containerOffsetY, fixture);
    echo.commitToken = container.operation ? container.operation->id : 0;
    Virtualizer::update(container, echo);
    CHECK_EQ(container.anchor.key, std::string("k30"));
    CHECK_NEAR(offsetOf(container, 33) - container.revision.containerOffsetY, k30Screen, 0.5);
  }
}

TEST(scroll_to_index_lands_on_the_requested_row) {
  Fixture fixture;
  std::vector<std::string> keys = keysFor(500);
  Container container;
  Virtualizer::update(container, inputFor(keys, 0.0, fixture));
  measureRows(container, std::vector<double>(keys.size(), 100.0));

  container.scrollToIndex(321);

  double offset = 0.0;
  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(container, inputFor(keys, offset, fixture));
    offset = container.revision.containerOffsetY;
  }

  std::size_t targetIndex = container.findElementIndexByKey("k321");
  CHECK_NEAR(offset, offsetOf(container, targetIndex), 1.0);
  checkNoRowLost(container, "after scrollToIndex");
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
    offset = container.revision.containerOffsetY;
  }

  double maxOffset = std::max(0.0, container.revision.totalContainerHeight - WINDOW_HEIGHT);
  CHECK(offset <= maxOffset + 1.0);
  checkNoRowLost(container, "after collapse");
}
