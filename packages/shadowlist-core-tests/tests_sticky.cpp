// Sticky header/footer: the template stays pinned to the viewport edge instead
// of scrolling with the content. The core only changes where the header/footer
// template is positioned (getHeaderOffset / getFooterOffset); the reserved
// header/footer space in the content size is unchanged, so the template settles
// back onto it at the scroll extremes.

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;

// Without sticky, the header rests at the content start (0) and the footer rests
// after the content (totalSize - footerSize), regardless of scroll position.
TEST(non_sticky_header_footer_rest_in_content) {
  Sim sim;
  sim.winH = 600;
  sim.headerSize = 80;
  sim.footerSize = 40;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();
  sim.scrollTo(3000.0);

  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 0.0, 0.5);
  // total = (header 80 + 100*100) + footer 40 = 10120; footer rests at 10120-40.
  CHECK_NEAR(sim.totalAxis(), 10120.0, 0.5);
  CHECK_NEAR(sim.container.getStickyFooterOffset(sim.footerSize), 10080.0, 0.5);
  CHECK_NEAR(sim.container.getFooterOffset(sim.footerSize), 10080.0, 0.5);
}

// A sticky header tracks the scroll offset so it stays at the viewport start.
TEST(sticky_header_tracks_scroll_offset) {
  Sim sim;
  sim.winH = 600;
  sim.headerSize = 80;
  sim.stickyHeader = true;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 0.0, 0.5);  // at the top

  sim.scrollTo(3000.0);
  CHECK_NEAR(sim.offsetY, 3000.0, 0.5);                     // offset is stable
  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 3000.0, 0.5); // header followed it
}

// A sticky footer tracks the scroll offset so it stays at the viewport end:
// offset + window - footerSize.
TEST(sticky_footer_pinned_to_viewport_end) {
  Sim sim;
  sim.winH = 600;
  sim.footerSize = 40;
  sim.stickyFooter = true;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  // At the top the footer floats at the viewport bottom (600 - 40), not at the
  // far end of the content.
  CHECK_NEAR(sim.container.getStickyFooterOffset(sim.footerSize), 560.0, 0.5);

  sim.scrollTo(3000.0);
  CHECK_NEAR(sim.container.getStickyFooterOffset(sim.footerSize), 3000.0 + 600.0 - 40.0, 0.5);
}

// At the very bottom the sticky footer's tracked position equals its resting
// position (totalSize - footerSize), so it settles seamlessly onto the reserved
// footer space instead of jumping.
TEST(sticky_footer_settles_at_list_bottom) {
  Sim sim;
  sim.winH = 600;
  sim.footerSize = 40;
  sim.stickyFooter = true;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  // total = 100*100 + footer 40 = 10040; maxOffset = 10040 - 600 = 9440.
  double total = sim.totalAxis();
  double maxOffset = total - sim.winH;
  sim.scrollTo(maxOffset);

  CHECK_NEAR(sim.container.getStickyFooterOffset(sim.footerSize), total - sim.footerSize, 0.5);
}

// Sticky is orientation aware: a horizontal sticky header tracks the horizontal
// scroll offset (reads offsetX / windowContainerWidth via getContainerOffset).
TEST(sticky_header_horizontal_tracks_offset_x) {
  Sim sim;
  sim.horizontal = true;
  sim.winW = 400;
  sim.headerSize = 80;
  sim.stickyHeader = true;
  sim.estimated = {100, 300};
  sim.sizeOfKey = [](const std::string&) { return Size{100, 300}; };
  sim.setKeys(makeKeys(100));
  sim.settle();

  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 0.0, 0.5);

  sim.offsetX = 2000.0;
  sim.settle();
  CHECK_NEAR(sim.container.getStickyHeaderOffset(), 2000.0, 0.5);
}

// The header's real size is measured one commit after the first layout (its inner
// content reports its size a frame late), so the header feeds back as 0 on commit 1
// and its real value on commit 2. The element offsets shift down by the header size
// between the two commits; MVCP must NOT treat that as a content scroll, otherwise it
// scrolls the list down by the header size and the list opens with the header hidden
// and the first item at the top. Regression for "list opens scrolled past header".
TEST(header_measured_late_keeps_list_at_top) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(30));

  // Commit 1: header template not measured yet (size 0).
  sim.headerSize = 0.0;
  sim.frame();

  // Commit 2: header measured (size 50) - the one-commit-late measurement.
  sim.headerSize = 50.0;
  sim.frame();
  sim.settle();

  // The list must still be at the top showing the header, not scrolled down by 50.
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);
  // Element 0 rests just below the header.
  CHECK_NEAR(sim.elementOffset(0), 50.0, 0.5);
}

// A genuine prepend at a scrolled position must still maintain the visible content
// position (the header-size compensation must not disable real MVCP).
TEST(header_compensation_preserves_prepend_mvcp) {
  Sim sim;
  sim.winH = 600;
  sim.headerSize = 50.0;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(30));
  sim.settle();

  // Scroll into the list, then prepend 5 rows above the viewport.
  sim.scrollTo(1000.0);
  std::string anchorKey = "k10";
  double anchorBefore = sim.elementOffset(sim.indexOfKey(anchorKey)) - sim.offsetY;

  std::vector<std::string> prepended = makeKeys(5, "p");
  for (const auto& k : makeKeys(30)) prepended.push_back(k);
  sim.setKeys(prepended);
  sim.settle();

  // The anchor row stays at the same viewport position (offset grew by the 5 rows).
  double anchorAfter = sim.elementOffset(sim.indexOfKey(anchorKey)) - sim.offsetY;
  CHECK_NEAR(anchorAfter, anchorBefore, 1.0);
}

// Faithful model of the REAL Fabric commit sequence: update() runs in adopt() and
// sees the header size from the PREVIOUS layout (stale), and the freshly measured
// header only lands in THIS commit's layout pass (recomputeElementOffsets +
// updateElementAtIndex), AFTER update() already measured with the stale size. The
// list must still open at offset 0 with the header visible and element 0 below it.
TEST(header_late_real_commit_sequence_keeps_top) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  sim.sizeOfKey = [](const std::string&) { return Size{400, 100}; };
  sim.setKeys(makeKeys(30));

  // Commit 1: header not measured yet (update and layout both see 0).
  sim.headerSize = 0.0;
  sim.frame();

  // Commit 2: update() still sees the stale header (0); the real 50 arrives only in
  // the layout pass below.
  FrameInput input = sim.makeInput();
  input.headerSize = 0.0;
  sim.virtualizer.update(&sim.container, input);

  // Layout pass: apply the freshly measured header, re-flow, feed visible sizes.
  sim.container.headerSize = 50.0;
  Virtualizer::recomputeElementOffsets(&sim.container, 0);
  std::size_t lo = 0, hi = 0;
  if (sim.visibleRange(lo, hi)) {
    for (std::size_t i = lo; i <= hi && i < sim.container.getElementsSize(); ++i) {
      Virtualizer::updateElementAtIndex(&sim.container, i, Size{400, 100});
    }
  }
  Virtualizer::recomputeTotalSize(&sim.container);

  // The shadow node re-asserts the offset on the header-size-change commit so the host
  // actually applies the resting offset (otherwise the re-flowed rows shift but the
  // scroll view's offset is left stale -> header scrolled past / overlapped).
  sim.container.containerOffsetCorrected = true;

  auto upd = sim.container.resolveStateUpdate(sim.offsetX, sim.offsetY, sim.prevTotalW, sim.prevTotalH);
  if (upd.applyContainerOffset) { sim.offsetX = upd.containerOffsetX; sim.offsetY = upd.containerOffsetY; }
  sim.prevTotalW = upd.totalContainerWidth; sim.prevTotalH = upd.totalContainerHeight;

  // The host must be told to apply the offset, and it must be the top (0) with element
  // 0 resting just below the header.
  CHECK_EQ(upd.applyContainerOffset, true);
  CHECK_NEAR(sim.offsetY, 0.0, 0.5);
  CHECK_NEAR(sim.elementOffset(0), 50.0, 0.5);
}
