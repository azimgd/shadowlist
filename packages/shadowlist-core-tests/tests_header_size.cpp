/*
 * Header size changes applied by the host's layout pass.
 * Fabric measures the header after update() already ran with the old header size, and
 * reflows the rows there. A loading spinner in the header often toggles in the same commit
 * that prepends the page it was loading. The rows on screen must not move in any frame:
 * not in that layout pass, not in the commit that applies the offset, and not when the
 * host reports it back.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;
using namespace slt;

namespace {

const double SPINNER_HEADER = 127.0;
const double PLAIN_HEADER = 79.0;

FrameInput frame(const std::vector<std::string>& keys, double offset, double headerSize) {
  FrameInput input = inputFor(keys, offset);
  input.headerSize = headerSize;
  return input;
}

// Rows of a loaded history page, keyed older, measure this tall once laid out.
const double OLDER_ROW_HEIGHT = 174.0;

/*
 * How a layout pass treats the rows of a loaded page.
 */
enum class OlderRows {
  // Sized like every other row, at the estimate.
  AtEstimate,
  // Not mounted yet, so the core gets no size for them.
  Unmounted,
  // Mounting in this pass, sized to zero first and then to their real size.
  Mounting,
};

bool isOlder(const Element& element) {
  return element.key.rfind("older", 0) == 0;
}

/*
 * Acts like the layout pass after update(): apply the header, then report every mounted row's size.
 */
void layoutPass(Container& container, double headerSize, OlderRows olderRows = OlderRows::AtEstimate) {
  /*
   * A mounting row first lays out at zero height, and Fabric reports that size before it
   * reads the header.
   */
  if (olderRows == OlderRows::Mounting) {
    for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
      if (isOlder(container.revision.elements[index])) {
        Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, 0.0});
      }
    }
  }
  double previousHeaderSize = container.headerSize;
  if (previousHeaderSize != headerSize) {
    container.headerSize = headerSize;
    Virtualizer::recomputeElementOffsets(&container, 0);
    Virtualizer::applyHeaderSizeChange(&container, previousHeaderSize);
  }
  std::size_t lowestChangedIndex = UNDEFINED_INDEX;
  for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
    bool older = isOlder(container.revision.elements[index]);
    if (older && olderRows == OlderRows::Unmounted) {
      continue;
    }
    double height = older && olderRows == OlderRows::Mounting ? OLDER_ROW_HEIGHT : ESTIMATED_ROW_HEIGHT;
    if (Virtualizer::applyElementSize(&container, index, {WINDOW_WIDTH, height}) && index < lowestChangedIndex) {
      lowestChangedIndex = index;
    }
  }
  if (lowestChangedIndex != UNDEFINED_INDEX) {
    Virtualizer::commitElementSizes(&container, lowestChangedIndex);
  }
  Virtualizer::recomputeTotalSize(&container);
}

/*
 * The user scrolls to offset and stops, with a layout pass after each report.
 */
void scrollTo(Container& container, const std::vector<std::string>& keys, double offset, double headerSize) {
  FrameInput drag = frame(keys, offset, headerSize);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, drag);
  layoutPass(container, headerSize);
  Virtualizer::update(&container, frame(keys, offset, headerSize));
  layoutPass(container, headerSize);
}

/*
 * Open the list with the spinner in the header and rest at offset.
 */
void openWithSpinnerAt(Container& container, const std::vector<std::string>& keys, double offset) {
  Virtualizer::update(&container, frame(keys, 0.0, 0.0));
  layoutPass(container, SPINNER_HEADER);
  Virtualizer::update(&container, frame(keys, 0.0, SPINNER_HEADER));
  layoutPass(container, SPINNER_HEADER);
  if (offset > 0.0) {
    scrollTo(container, keys, offset, SPINNER_HEADER);
  }
}

double onScreen(const Container& container, const std::string& key) {
  return offsetOf(container, container.findElementIndexByKey(key)) - container.revision.containerOffsetY;
}

std::vector<std::string> prependedTo(const std::vector<std::string>& keys, std::size_t count) {
  std::vector<std::string> grown = keysFor(count, "older");
  grown.insert(grown.end(), keys.begin(), keys.end());
  return grown;
}

}

TEST(prepend_that_removes_the_header_spinner_keeps_the_rows_still_in_every_frame) {
  for (double restingOffset : {0.0, 400.0}) {
    std::vector<std::string> keys = keysFor(60);
    Container container;
    openWithSpinnerAt(container, keys, restingOffset);
    double before = onScreen(container, "k5");

    // The page lands while update() still sees the spinner in the header.
    std::vector<std::string> grown = prependedTo(keys, 6);
    Virtualizer::update(&container, frame(grown, restingOffset, SPINNER_HEADER));
    CHECK(container.operation.has_value());
    std::uint64_t token = container.operation ? container.operation->id : 0;
    CHECK_NEAR(onScreen(container, "k5"), before, 0.5);

    // Layout measures the header without the spinner. This frame used to show the jump.
    layoutPass(container, PLAIN_HEADER);
    CHECK_NEAR(onScreen(container, "k5"), before, 0.5);
    double published = container.revision.containerOffsetY;

    // The commit that applies the new offset must not correct it again.
    FrameInput adopt = frame(grown, published, PLAIN_HEADER);
    adopt.containerOffsetEnabled = true;
    adopt.commitToken = token;
    Virtualizer::update(&container, adopt);
    layoutPass(container, PLAIN_HEADER);
    CHECK_NEAR(container.revision.containerOffsetY, published, 0.5);
    CHECK_NEAR(onScreen(container, "k5"), before, 0.5);

    // Neither does the host reporting it back.
    FrameInput echo = frame(grown, published, PLAIN_HEADER);
    echo.commitToken = token;
    Virtualizer::update(&container, echo);
    layoutPass(container, PLAIN_HEADER);
    CHECK_NEAR(container.revision.containerOffsetY, published, 0.5);
    CHECK_NEAR(onScreen(container, "k5"), before, 0.5);
  }
}

/*
 * The loading cycle repeats at the top. The page lands and hides the spinner, the start is
 * still close so the spinner comes right back, and the new page measures taller than its
 * estimate in that same layout pass, all while the page's correction is still running.
 */
TEST(spinner_returning_while_the_landed_page_measures_keeps_the_rows_still) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  openWithSpinnerAt(container, keys, 0.0);
  double before = onScreen(container, "k2");

  std::vector<std::string> grown = prependedTo(keys, 6);
  Virtualizer::update(&container, frame(grown, 0.0, SPINNER_HEADER));
  std::uint64_t token = container.operation ? container.operation->id : 0;
  // The page has not mounted yet, so its rows keep their estimates here.
  layoutPass(container, PLAIN_HEADER, OlderRows::Unmounted);
  CHECK_NEAR(onScreen(container, "k2"), before, 0.5);

  double published = container.revision.containerOffsetY;
  FrameInput adopt = frame(grown, published, PLAIN_HEADER);
  adopt.containerOffsetEnabled = true;
  adopt.commitToken = token;
  Virtualizer::update(&container, adopt);
  layoutPass(container, PLAIN_HEADER, OlderRows::Unmounted);
  CHECK_NEAR(onScreen(container, "k2"), before, 0.5);

  // The spinner comes back in the pass that mounts the new page.
  layoutPass(container, SPINNER_HEADER, OlderRows::Mounting);
  CHECK(container.operation.has_value());
  CHECK_NEAR(onScreen(container, "k2"), before, 0.5);

  published = container.revision.containerOffsetY;
  adopt = frame(grown, published, SPINNER_HEADER);
  adopt.containerOffsetEnabled = true;
  adopt.commitToken = container.operation ? container.operation->id : 0;
  Virtualizer::update(&container, adopt);
  layoutPass(container, SPINNER_HEADER);
  CHECK_NEAR(onScreen(container, "k2"), before, 0.5);

  FrameInput echo = frame(grown, container.revision.containerOffsetY, SPINNER_HEADER);
  echo.commitToken = adopt.commitToken;
  Virtualizer::update(&container, echo);
  layoutPass(container, SPINNER_HEADER);
  CHECK_NEAR(onScreen(container, "k2"), before, 0.5);
}

TEST(header_resizing_while_scrolled_out_of_view_keeps_the_rows_still) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  openWithSpinnerAt(container, keys, 900.0);
  double before = onScreen(container, "k10");

  layoutPass(container, PLAIN_HEADER);
  CHECK(container.containerOffsetCorrected);
  CHECK_NEAR(onScreen(container, "k10"), before, 0.5);

  double published = container.revision.containerOffsetY;
  Virtualizer::update(&container, frame(keys, published, PLAIN_HEADER));
  layoutPass(container, PLAIN_HEADER);
  CHECK_NEAR(container.revision.containerOffsetY, published, 0.5);
  CHECK_NEAR(onScreen(container, "k10"), before, 0.5);
}

TEST(header_growing_on_screen_pushes_the_rows_below_it) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  Virtualizer::update(&container, frame(keys, 0.0, 0.0));
  layoutPass(container, 0.0);
  CHECK_NEAR(onScreen(container, "k0"), 0.0, 0.5);

  layoutPass(container, SPINNER_HEADER);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 0.5);
  CHECK_NEAR(onScreen(container, "k0"), SPINNER_HEADER, 0.5);

  Virtualizer::update(&container, frame(keys, 0.0, SPINNER_HEADER));
  layoutPass(container, SPINNER_HEADER);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 0.5);
  CHECK_NEAR(onScreen(container, "k0"), SPINNER_HEADER, 0.5);
}

/*
 * scrollToStart after a prepend. The old first row first stays in place with the new rows
 * above the screen, then the command lands on offset 0 with the header showing.
 */
TEST(scroll_to_start_after_a_prepend_lands_on_offset_zero_with_the_header) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, frame(keys, 0.0, PLAIN_HEADER));
  layoutPass(container, PLAIN_HEADER);

  std::vector<std::string> prepended = keysFor(10, "new");
  prepended.insert(prepended.end(), keys.begin(), keys.end());
  Virtualizer::update(&container, frame(prepended, 0.0, PLAIN_HEADER));
  layoutPass(container, PLAIN_HEADER);
  double anchored = container.revision.containerOffsetY;
  CHECK(anchored > PLAIN_HEADER);

  container.scrollToStart();
  Virtualizer::update(&container, frame(prepended, anchored, PLAIN_HEADER));
  layoutPass(container, PLAIN_HEADER);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 0.5);
  CHECK_NEAR(onScreen(container, "new0"), PLAIN_HEADER, 0.5);
}

/*
 * Momentum reports that arrive before the host applies scrollToStart neither cancel it nor
 * move its target. The host stops the fling when it applies the correction.
 */
TEST(scroll_to_start_keeps_its_target_across_momentum_reports) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, frame(keys, 0.0, PLAIN_HEADER));
  layoutPass(container, PLAIN_HEADER);
  FrameInput deep = frame(keys, 2000.0, PLAIN_HEADER);
  deep.userScrolled = true;
  deep.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, deep);
  layoutPass(container, PLAIN_HEADER);

  container.scrollToStart();
  Virtualizer::update(&container, frame(keys, 2000.0, PLAIN_HEADER));
  layoutPass(container, PLAIN_HEADER);
  CHECK(container.operation.has_value());
  std::uint64_t token = container.operation->id;

  FrameInput coasting = frame(keys, 2100.0, PLAIN_HEADER);
  coasting.userScrolled = true;
  coasting.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, coasting);
  layoutPass(container, PLAIN_HEADER);
  CHECK(container.operation.has_value() && container.operation->id == token);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 0.5);
}

// A finger drag cancels a scrollToEnd, but momentum scrolling does not.
TEST(scroll_to_end_yields_to_a_drag_but_not_to_momentum) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  Virtualizer::update(&container, frame(keys, 0.0, PLAIN_HEADER));
  layoutPass(container, PLAIN_HEADER);

  container.scrollToEnd();
  FrameInput drag = frame(keys, 400.0, PLAIN_HEADER);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, drag);
  layoutPass(container, PLAIN_HEADER);
  CHECK(!container.pendingScrollToEnd);
  CHECK(!container.operation.has_value() || container.operation->type != OperationType::ScrollToEnd);

  container.scrollToEnd();
  FrameInput coasting = frame(keys, 600.0, PLAIN_HEADER);
  coasting.userScrolled = true;
  coasting.scrollPhase = ScrollPhase::Settling;
  Virtualizer::update(&container, coasting);
  layoutPass(container, PLAIN_HEADER);
  CHECK(container.operation.has_value() && container.operation->type == OperationType::ScrollToEnd);
}

/*
 * ShadowListNative setData with scrollTo start sends the new rows and scrollToStart in one
 * update. The command wins over keeping the visible row in place, so the only correction is
 * to offset 0. Without the command the same update holds the old row.
 */
TEST(scroll_to_start_with_new_rows_is_one_correction) {
  std::vector<std::string> keys = keysFor(40);
  std::vector<std::string> replaced = keysFor(10, "new");
  replaced.insert(replaced.end(), keys.begin(), keys.end());

  auto deepContainer = [&](Container& container) {
    Virtualizer::update(&container, frame(keys, 0.0, PLAIN_HEADER));
    layoutPass(container, PLAIN_HEADER);
    Virtualizer::update(&container, frame(keys, 2000.0, PLAIN_HEADER));
    layoutPass(container, PLAIN_HEADER);
  };

  Container held;
  deepContainer(held);
  Virtualizer::update(&held, frame(replaced, 2000.0, PLAIN_HEADER));
  CHECK(held.operation.has_value() && held.operation->type == OperationType::MaintainAnchor);

  Container container;
  deepContainer(container);
  container.scrollToStart();
  FrameInput moving = frame(replaced, 2000.0, PLAIN_HEADER);
  Virtualizer::update(&container, moving);
  CHECK(container.operation.has_value() && container.operation->type == OperationType::ScrollToStart);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 0.5);
  layoutPass(container, PLAIN_HEADER);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 0.5);
  CHECK_NEAR(onScreen(container, "new0"), PLAIN_HEADER, 0.5);
}

/*
 * The same setData on a list resting at offset 0, where pull to refresh leaves it. The command
 * is already at its target in the frame that asked for it, and must stop there. Otherwise the
 * old first row would be held, and the command would only work if the host reported exactly 0.
 */
TEST(scroll_to_start_with_new_rows_at_offset_zero_stays_at_zero) {
  std::vector<std::string> keys = keysFor(40);
  std::vector<std::string> replaced = keysFor(10, "new");
  replaced.insert(replaced.end(), keys.begin(), keys.end());

  Container container;
  Virtualizer::update(&container, frame(keys, 0.0, PLAIN_HEADER));
  layoutPass(container, PLAIN_HEADER);
  Virtualizer::update(&container, frame(keys, 0.0, PLAIN_HEADER));
  layoutPass(container, PLAIN_HEADER);

  container.scrollToStart();
  Virtualizer::update(&container, frame(replaced, 0.0, PLAIN_HEADER));
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 0.5);
  layoutPass(container, PLAIN_HEADER);
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 0.5);
  CHECK_NEAR(onScreen(container, "new0"), PLAIN_HEADER, 0.5);

  // The next commits keep it at 0 and do not go back to the old first row.
  for (int commit = 0; commit < 3; ++commit) {
    Virtualizer::update(&container, frame(replaced, container.revision.containerOffsetY, PLAIN_HEADER));
    layoutPass(container, PLAIN_HEADER);
  }
  CHECK_NEAR(container.revision.containerOffsetY, 0.0, 0.5);
  CHECK_NEAR(onScreen(container, "new0"), PLAIN_HEADER, 0.5);
}
