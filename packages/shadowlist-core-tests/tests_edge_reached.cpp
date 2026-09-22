/*
 * Edge callback tests: when onStartReached and onEndReached fire while the reader keeps
 * scrolling toward that edge and the data keeps growing.
 * The case here is chat history. The reader flings up, each call loads a page of older
 * messages, and the page lands a few frames later. The list must keep asking as long as
 * the reader keeps scrolling toward the edge, even if a page lands while the app is busy.
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
 * Fling up for a number of frames over a list that loads older rows at its start,
 * and return how many pages arrived. busyConsumer drops a request made while a fetch
 * is still running, like every real paging hook does.
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

  // Start at the newest message, like the chat does.
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
      // Commit the new page at the offset the core last settled on.
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

  // Every request was answered, none twice and none lost.
  CHECK_EQ(requested, pagesDelivered);
  return pagesDelivered;
}

}

/*
 * The baseline: the app answers every request, and the list loads enough history
 * to cover the distance scrolled.
 */
TEST(upward_fling_pages_history_while_the_consumer_keeps_up) {
  int pages = flingUpCountingPages(400, 600.0, false);
  // 240000 pixels of scrolling at 6000 pixels per page needs at least 30 pages.
  CHECK(pages >= 30);
}

/*
 * The real case. A paging hook drops a request that lands during a fetch, so the list
 * must ask again while the reader keeps scrolling, or the history stops arriving.
 */
TEST(upward_fling_keeps_paging_when_a_request_lands_mid_fetch) {
  int busyPages = flingUpCountingPages(400, 600.0, true);
  int freePages = flingUpCountingPages(400, 600.0, false);
  // Answering one request at a time loses no history.
  CHECK_EQ(busyPages, freePages);
}

/*
 * scrollToIndex can place the row anywhere on screen, not only at the top. A chat jumping
 * to a quoted message wants it centered with its context around it.
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

  // 0 is the default and puts the row's top at the top of the screen.
  container.requestScrollToIndex(static_cast<double>(target), 1.0, -2, 0.0);
  double atStart = settle();
  CHECK_NEAR(atStart, container.getElementOffset(target), 1.0);

  // 0.5 centers it, with half the free space above it.
  container.requestScrollToIndex(static_cast<double>(target), 2.0, -2, 0.5);
  double centred = settle();
  double freeSpace = WINDOW_HEIGHT - ROW_HEIGHT;
  CHECK_NEAR(centred, container.getElementOffset(target) - freeSpace * 0.5, 1.0);

  // 1 puts the row's bottom at the bottom of the screen.
  container.requestScrollToIndex(static_cast<double>(target), 3.0, -2, 1.0);
  double atEnd = settle();
  CHECK_NEAR(atEnd, container.getElementOffset(target) - freeSpace, 1.0);
}

/*
 * A position below 0 or above 1 would put the row off screen, so it is clamped to the
 * nearer edge and the row stays visible.
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
 * A jump to an unmeasured row aims at its estimated size, and the real size only arrives
 * once the row mounts. The free space is worked out again every frame, so the row still
 * ends up where its real size puts it.
 */
TEST(scroll_to_index_view_position_converges_as_the_target_is_measured) {
  constexpr double TALL_ROW = 400.0;
  const std::size_t target = 120;
  std::vector<std::string> keys = keysFor(200, "m");
  Container container;

  Virtualizer::update(&container, chatInput(keys, 0.0));
  // Only the first rows are measured, so the target still uses its estimate.
  measureAll(container, 10);
  Virtualizer::update(&container, chatInput(keys, 0.0));

  container.requestScrollToIndex(static_cast<double>(target), 1.0, -2, 0.5);

  for (int frame = 0; frame < 8; ++frame) {
    Virtualizer::update(&container, chatInput(keys, container.revision.containerOffsetY));
    // Once mounted, the target reports a size far from the estimate.
    Virtualizer::updateElementAtIndex(&container, target, {WINDOW_WIDTH, TALL_ROW});
  }

  double freeSpace = WINDOW_HEIGHT - TALL_ROW;
  CHECK_NEAR(container.revision.containerOffsetY,
    container.getElementOffset(target) - freeSpace * 0.5, 1.0);
}

/*
 * A scroll command can run before the window has a size, when there is no free space to
 * share out. The position must be applied again once the real window size arrives.
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
