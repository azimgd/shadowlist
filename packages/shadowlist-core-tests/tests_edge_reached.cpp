/*
 * Edge-callback tests: when onStartReached / onEndReached fire while the reader keeps
 * travelling toward that edge and the data keeps growing under them.
 *
 * The case these pin down is chat history. The reader flings up through a conversation,
 * each fire loads a page of older messages, and the page lands a few frames later (a
 * network round trip). The contract is that the list keeps asking for as long as the
 * reader keeps travelling into the band -- a page that arrives while the consumer was
 * busy must not cost the reader the rest of their history.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

constexpr double ROW_HEIGHT = 100.0;
constexpr double START_THRESHOLD = 0.5;
// A page of chat history, and how many frames the fetch takes to come back.
constexpr std::size_t PAGE_ROWS = 60;
constexpr int FETCH_FRAMES = 3;

void measureAll(Container& container, std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, ROW_HEIGHT});
  }
}

FrameInput chatInput(const std::vector<std::string>& keys, double offset) {
  FrameInput input;
  input.keys = keys;
  input.windowContainerWidth = WINDOW_WIDTH;
  input.windowContainerHeight = WINDOW_HEIGHT;
  input.columns = 1;
  input.overscan = 1.0;
  input.estimatedElementSize = {WINDOW_WIDTH, ROW_HEIGHT};
  input.containerOffsetY = offset;
  input.startReachedThreshold = START_THRESHOLD;
  input.endReachedThreshold = 0.1;
  return input;
}

/*
 * Drive `frames` of a steady upward fling over a list that pages in older rows at its
 * start, and report how many pages the list asked for. `pageLatency` models a consumer
 * that cannot answer a second request while the first is in flight -- which is every
 * real paging hook.
 */
int flingUpCountingPages(int frames, double pixelsPerFrame, bool busyConsumer) {
  std::vector<std::string> keys = keysFor(PAGE_ROWS, "m");
  Container container;

  int requested = 0;
  bool fetchInFlight = false;
  int fetchDueIn = 0;
  int pagesDelivered = 0;

  container.onStartReachedCallback = [&]() {
    requested++;
    if (busyConsumer && fetchInFlight) return;
    fetchInFlight = true;
    fetchDueIn = FETCH_FRAMES;
  };

  Virtualizer::update(&container, chatInput(keys, 0.0));
  measureAll(container, keys.size());

  // Open resting on the newest message, the way the chat does.
  double bottom = container.revision.totalContainerHeight - WINDOW_HEIGHT;
  FrameInput settle = chatInput(keys, bottom);
  Virtualizer::update(&container, settle);

  for (int frame = 0; frame < frames; ++frame) {
    if (fetchInFlight && --fetchDueIn <= 0) {
      std::vector<std::string> older =
        keysFor(PAGE_ROWS, "p" + std::to_string(pagesDelivered) + "_");
      older.insert(older.end(), keys.begin(), keys.end());
      keys = older;
      pagesDelivered++;
      fetchInFlight = false;
      // The page commit itself, at whatever offset the core last settled on.
      FrameInput commit = chatInput(keys, container.revision.containerOffsetY);
      Virtualizer::update(&container, commit);
      measureAll(container, keys.size());
      Virtualizer::update(&container, chatInput(keys, container.revision.containerOffsetY));
    }

    double offset = container.revision.containerOffsetY - pixelsPerFrame;
    if (offset < 0.0) offset = 0.0;
    FrameInput input = chatInput(keys, offset);
    input.userScrolled = true;
    Virtualizer::update(&container, input);
  }

  // Every request the list made was answered: nothing was asked for twice, nothing lost.
  CHECK_EQ(requested, pagesDelivered);
  return pagesDelivered;
}

}

/*
 * The control: a consumer that answers every request. The reader travels a fixed
 * distance and the list pages in enough history to cover it.
 */
TEST(upward_fling_pages_history_while_the_consumer_keeps_up) {
  int pages = flingUpCountingPages(400, 600.0, false);
  // 240,000px of travel over rows PAGE_ROWS x ROW_HEIGHT tall: the list covers it.
  CHECK(pages >= 30);
}

/*
 * The real one. A paging hook cannot start a second fetch while the first is running, so
 * a fire that lands mid-fetch is dropped. The list must ask again once the reader is
 * still travelling into the band, or the history simply stops arriving.
 */
TEST(upward_fling_keeps_paging_when_a_request_lands_mid_fetch) {
  int busyPages = flingUpCountingPages(400, 600.0, true);
  int freePages = flingUpCountingPages(400, 600.0, false);
  // A consumer that can only answer one request at a time loses no history to it.
  CHECK_EQ(busyPages, freePages);
}

/*
 * scrollToIndex places the row within the viewport, not only at its leading edge: a chat
 * jumping to the message a reply quotes wants it centred, with the conversation that
 * gives it context around it rather than off screen above.
 */
TEST(scroll_to_index_places_the_row_at_the_requested_view_position) {
  std::vector<std::string> keys = keysFor(120, "m");
  Container container;

  Virtualizer::update(&container, chatInput(keys, 0.0));
  measureAll(container, keys.size());
  Virtualizer::update(&container, chatInput(keys, 0.0));

  const std::size_t target = 60;
  auto settle = [&]() {
    for (int frame = 0; frame < 6; ++frame) {
      Virtualizer::update(&container, chatInput(keys, container.revision.containerOffsetY));
    }
    return container.revision.containerOffsetY;
  };

  // 0 (the default) keeps the historical behaviour: the row's top edge at the viewport top.
  container.requestScrollToIndex(static_cast<double>(target), 1.0, -2, 0.0);
  double atStart = settle();
  CHECK_NEAR(atStart, container.getElementOffset(target), 1.0);

  // 0.5 centres it: half of the space the row leaves free sits above it.
  container.requestScrollToIndex(static_cast<double>(target), 2.0, -2, 0.5);
  double centred = settle();
  double freeSpace = WINDOW_HEIGHT - ROW_HEIGHT;
  CHECK_NEAR(centred, container.getElementOffset(target) - freeSpace * 0.5, 1.0);

  // 1 aligns it to the end: the row's bottom edge at the viewport bottom.
  container.requestScrollToIndex(static_cast<double>(target), 3.0, -2, 1.0);
  double atEnd = settle();
  CHECK_NEAR(atEnd, container.getElementOffset(target) - freeSpace, 1.0);
}

/*
 * A fraction outside [0, 1] would put the row off screen, so it is clamped to the nearer
 * edge rather than honoured: a caller passing rubbish gets the row on screen, not a blank
 * viewport.
 */
TEST(scroll_to_index_clamps_a_view_position_outside_the_viewport) {
  std::vector<std::string> keys = keysFor(120, "m");
  Container container;

  Virtualizer::update(&container, chatInput(keys, 0.0));
  measureAll(container, keys.size());
  Virtualizer::update(&container, chatInput(keys, 0.0));

  const std::size_t target = 60;
  auto settle = [&]() {
    for (int frame = 0; frame < 6; ++frame) {
      Virtualizer::update(&container, chatInput(keys, container.revision.containerOffsetY));
    }
    return container.revision.containerOffsetY;
  };

  container.requestScrollToIndex(static_cast<double>(target), 1.0, -2, 4.0);
  double clampedHigh = settle();
  CHECK_NEAR(clampedHigh, container.getElementOffset(target) - (WINDOW_HEIGHT - ROW_HEIGHT), 1.0);

  container.requestScrollToIndex(static_cast<double>(target), 2.0, -2, -3.0);
  double clampedLow = settle();
  CHECK_NEAR(clampedLow, container.getElementOffset(target), 1.0);
}

/*
 * A jump into unmeasured territory targets the row's ESTIMATE, and the row is measured only
 * once the layout pass mounts it. Freezing the free space derived from that estimate leaves
 * the row off-position by viewPosition of its error; rederiving it per frame lands the row
 * where its true size implies.
 */
TEST(scroll_to_index_view_position_converges_as_the_target_is_measured) {
  constexpr double TALL_ROW = 400.0;
  const std::size_t target = 120;
  std::vector<std::string> keys = keysFor(200, "m");
  Container container;

  Virtualizer::update(&container, chatInput(keys, 0.0));
  // Only the opening rows have been laid out; the target is still on its estimate.
  measureAll(container, 10);
  Virtualizer::update(&container, chatInput(keys, 0.0));

  container.requestScrollToIndex(static_cast<double>(target), 1.0, -2, 0.5);

  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(&container, chatInput(keys, container.revision.containerOffsetY));
    // The layout pass that mounts the target reports a size nothing like the estimate.
    Virtualizer::updateElementAtIndex(&container, target, {WINDOW_WIDTH, TALL_ROW});
  }

  double freeSpace = WINDOW_HEIGHT - TALL_ROW;
  CHECK_NEAR(container.revision.containerOffsetY,
    container.getElementOffset(target) - freeSpace * 0.5, 1.0);
}

/*
 * A command can resolve on a commit taken before the window is measured, where there is no
 * free space to distribute. The position must be reapplied once a real window size arrives.
 */
TEST(scroll_to_index_view_position_survives_a_commit_before_the_window_is_measured) {
  const std::size_t target = 60;
  std::vector<std::string> keys = keysFor(120, "m");
  Container container;

  FrameInput unmeasured = chatInput(keys, 0.0);
  unmeasured.windowContainerHeight = 0.0;
  Virtualizer::update(&container, unmeasured);

  container.requestScrollToIndex(static_cast<double>(target), 1.0, -2, 1.0);
  Virtualizer::update(&container, unmeasured);

  measureAll(container, keys.size());
  for (int frame = 0; frame < 6; ++frame) {
    Virtualizer::update(&container, chatInput(keys, container.revision.containerOffsetY));
  }

  CHECK_NEAR(container.revision.containerOffsetY,
    container.getElementOffset(target) - (WINDOW_HEIGHT - ROW_HEIGHT), 1.0);
}
