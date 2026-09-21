/*
 * Header size changes applied by a host's layout pass.
 *
 * Fabric measures the header template in its layout pass, after update() already ran for the
 * commit with the previous header size, and reflows the rows for it there. A loading spinner in
 * the header typically toggles in the very commit that prepends the page it was loading. The
 * rows on screen must not move in any frame of that sequence: not in the layout pass that
 * applies the new header, not in the commit that adopts the published offset, and not when the
 * host echoes it.
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

// Rows prefixed "older" (a loaded history page) measure this tall once laid out.
const double OLDER_ROW_HEIGHT = 174.0;

// How a layout pass treats the rows of a loaded page ("older" keys).
enum class OlderRows {
  // Laid out like every other row, at the estimate.
  AtEstimate,
  // Not mounted yet: no measurement reaches the core.
  Unmounted,
  // Mounting in this pass: laid out to zero first, then at their real size.
  Mounting,
};

bool isOlder(const Element& element) {
  return element.key.rfind("older", 0) == 0;
}

/*
 * What the layout pass does after update(): apply the measured header, then feed back every
 * mounted row's size.
 */
void layoutPass(Container& container, double headerSize, OlderRows olderRows = OlderRows::AtEstimate) {
  /*
   * A row mounting in this layout first lays out to zero, and that frame is fed back while
   * the children are laid out, before the header is read (Fabric's replaceChild).
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

// A user scroll to `offset`, then the idle report once it stopped, each followed by its layout.
void scrollTo(Container& container, const std::vector<std::string>& keys, double offset, double headerSize) {
  FrameInput drag = frame(keys, offset, headerSize);
  drag.userScrolled = true;
  drag.scrollPhase = ScrollPhase::Dragging;
  Virtualizer::update(&container, drag);
  layoutPass(container, headerSize);
  Virtualizer::update(&container, frame(keys, offset, headerSize));
  layoutPass(container, headerSize);
}

// A list opened, laid out with the spinner header, and resting at `offset`.
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

    // The page lands with the spinner still in the header this update() knows about.
    std::vector<std::string> grown = prependedTo(keys, 6);
    Virtualizer::update(&container, frame(grown, restingOffset, SPINNER_HEADER));
    CHECK(container.operation.has_value());
    std::uint64_t token = container.operation ? container.operation->id : 0;
    CHECK_NEAR(onScreen(container, "k5"), before, 0.5);

    // Layout measures the header without the spinner: this is the frame that showed the jump.
    layoutPass(container, PLAIN_HEADER);
    CHECK_NEAR(onScreen(container, "k5"), before, 0.5);
    double published = container.revision.containerOffsetY;

    // The commit adopting the published state must not correct again.
    FrameInput adopt = frame(grown, published, PLAIN_HEADER);
    adopt.containerOffsetEnabled = true;
    adopt.commitToken = token;
    Virtualizer::update(&container, adopt);
    layoutPass(container, PLAIN_HEADER);
    CHECK_NEAR(container.revision.containerOffsetY, published, 0.5);
    CHECK_NEAR(onScreen(container, "k5"), before, 0.5);

    // Nor the host's echo of it.
    FrameInput echo = frame(grown, published, PLAIN_HEADER);
    echo.commitToken = token;
    Virtualizer::update(&container, echo);
    layoutPass(container, PLAIN_HEADER);
    CHECK_NEAR(container.revision.containerOffsetY, published, 0.5);
    CHECK_NEAR(onScreen(container, "k5"), before, 0.5);
  }
}

/*
 * The loading cycle repeated at the top: the page lands and removes the spinner, the start is
 * still within reach so the spinner comes straight back, and the landed page measures taller
 * than its estimate in that same layout pass, all while the page's correction is in flight.
 */
TEST(spinner_returning_while_the_landed_page_measures_keeps_the_rows_still) {
  std::vector<std::string> keys = keysFor(60);
  Container container;
  openWithSpinnerAt(container, keys, 0.0);
  double before = onScreen(container, "k2");

  std::vector<std::string> grown = prependedTo(keys, 6);
  Virtualizer::update(&container, frame(grown, 0.0, SPINNER_HEADER));
  std::uint64_t token = container.operation ? container.operation->id : 0;
  // The page has not mounted yet: its rows keep their estimates in these passes.
  layoutPass(container, PLAIN_HEADER, OlderRows::Unmounted);
  CHECK_NEAR(onScreen(container, "k2"), before, 0.5);

  double published = container.revision.containerOffsetY;
  FrameInput adopt = frame(grown, published, PLAIN_HEADER);
  adopt.containerOffsetEnabled = true;
  adopt.commitToken = token;
  Virtualizer::update(&container, adopt);
  layoutPass(container, PLAIN_HEADER, OlderRows::Unmounted);
  CHECK_NEAR(onScreen(container, "k2"), before, 0.5);

  // The spinner returns in the pass that mounts the landed page.
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
 * scrollToStart after a prepend: MVCP first keeps the old first row where it was (the new rows
 * above the viewport), then the request lands on offset 0 with the header in view, not on the
 * first row's leading edge.
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
 * scrollToStart is a command: momentum reports that arrive before the host applies it (the host
 * stops the fling when it mounts the correction) neither cancel it nor move its target.
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

/*
 * A ShadowListNative scroll command's frame runs idle when it lands over momentum, but keeps a
 * finger's drag: the drag cancels a scrollToEnd, momentum does not.
 */
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
 * ShadowListNative's setData(items, { scrollTo: 'start' }): the new rows and scrollToStart reach
 * the core in one update. The command takes precedence over MVCP, so the only correction is the
 * one to offset 0; without it the same update holds the old anchor row.
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
