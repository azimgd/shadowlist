/*
 * Work the layout skips because it can't change anything. A window that only grows or
 * shrinks along the scroll axis, like a chat composer resizing the list, moves no row,
 * so the rows are not walked again. These tests check the layout still comes out the
 * same as a full reflow would give, and that changes that do move rows still reflow.
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

double heightAt(std::size_t index) {
  return 60.0 + static_cast<double>((index * 53) % 200);
}

/*
 * Every row must sit right after the one before it, starting below the header, and know
 * its own index. That is what a full reflow from row 0 gives.
 */
void checkSingleTrack(const Container& container) {
  double expected = container.headerSize;
  for (std::size_t index = 0; index < container.revision.elements.size(); ++index) {
    const Element& element = container.revision.elements[index];
    CHECK_NEAR(element.offsetY, expected, 0.0001);
    CHECK_EQ(element.index, index);
    expected += element.height;
  }
}

}

TEST(a_scroll_axis_window_resize_keeps_every_row_in_place) {
  std::vector<std::string> keys = keysFor(300);
  Container container;

  FrameInput input = inputFor(keys, 0.0);
  input.headerSize = 44.0;
  Virtualizer::update(&container, input);

  // Measure a first screen, then scroll and resize the window along the scroll axis.
  for (std::size_t index = 0; index < 20; ++index) {
    Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, heightAt(index)});
  }
  double heights[] = {600.0, 840.0, 512.0, 840.0, 700.0};
  double offset = 0.0;
  for (double windowHeight : heights) {
    offset += 900.0;
    input.containerOffsetY = offset;
    input.windowContainerHeight = windowHeight;
    Virtualizer::update(&container, input);
    auto [start, end] = container.getVisibleIndices();
    for (std::size_t index = start; index <= end && index < keys.size(); ++index) {
      Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, heightAt(index)});
    }
    Virtualizer::recomputeTotalSize(&container);
    checkSingleTrack(container);
  }
}

TEST(a_scroll_axis_resize_from_the_layout_pass_keeps_every_row_in_place) {
  std::vector<std::string> keys = keysFor(120);
  Container container;
  FrameInput input = inputFor(keys, 0.0);
  input.headerSize = 30.0;
  Virtualizer::update(&container, input);
  for (std::size_t index = 0; index < 12; ++index) {
    Virtualizer::updateElementAtIndex(&container, index, {WINDOW_WIDTH, heightAt(index)});
  }
  Virtualizer::recomputeTotalSize(&container);

  /*
   * Like the Fabric layout pass: it writes the new window height straight into the core
   * and only reflows when the header or the cross size changed. The next update() then
   * sees the new height and must not need a reflow either.
   */
  double previousWindowSize = container.getWindowContainerSize();
  container.revision.windowContainerHeight = 500.0;
  Virtualizer::applyWindowSizeChange(&container, previousWindowSize);
  input.windowContainerHeight = 500.0;
  Virtualizer::update(&container, input);
  checkSingleTrack(container);

  // A row resized after that still moves every row below it.
  Virtualizer::updateElementAtIndex(&container, 3, {WINDOW_WIDTH, 333.0});
  Virtualizer::recomputeTotalSize(&container);
  checkSingleTrack(container);
}

TEST(a_header_change_still_reflows_after_a_scroll_axis_resize) {
  std::vector<std::string> keys = keysFor(80);
  Container container;
  FrameInput input = inputFor(keys, 0.0);
  Virtualizer::update(&container, input);
  input.windowContainerHeight = 600.0;
  Virtualizer::update(&container, input);
  input.headerSize = 90.0;
  Virtualizer::update(&container, input);
  checkSingleTrack(container);
  CHECK_NEAR(container.revision.elements[0].offsetY, 90.0, 0.0001);
}

TEST(a_cross_axis_resize_still_reflows_columns) {
  std::vector<std::string> keys = keysFor(40);
  Container container;
  FrameInput input = inputFor(keys, 0.0);
  input.columns = 2;
  Virtualizer::update(&container, input);
  CHECK_NEAR(container.revision.elements[1].width, WINDOW_WIDTH / 2.0, 0.0001);

  // Only the height changes, so the columns keep their width.
  input.windowContainerHeight = 500.0;
  Virtualizer::update(&container, input);
  CHECK_NEAR(container.revision.elements[1].width, WINDOW_WIDTH / 2.0, 0.0001);
  CHECK_NEAR(container.revision.elements[1].offsetX, WINDOW_WIDTH / 2.0, 0.0001);

  // A wider window widens the columns.
  input.windowContainerWidth = 600.0;
  Virtualizer::update(&container, input);
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const Element& element = container.revision.elements[index];
    CHECK_NEAR(element.width, 300.0, 0.0001);
    CHECK_NEAR(element.offsetX, (index % 2) * 300.0, 0.0001);
  }
}

TEST(a_horizontal_list_treats_height_as_the_cross_axis) {
  std::vector<std::string> keys = keysFor(30);
  Container container;
  FrameInput input = inputFor(keys, 0.0);
  input.horizontal = true;
  input.columns = 2;
  input.estimatedElementSize = {150.0, 100.0};
  Virtualizer::update(&container, input);
  CHECK_NEAR(container.revision.elements[1].height, WINDOW_HEIGHT / 2.0, 0.0001);

  // A new width is along the scroll axis and changes no track.
  input.windowContainerWidth = 300.0;
  Virtualizer::update(&container, input);
  CHECK_NEAR(container.revision.elements[1].height, WINDOW_HEIGHT / 2.0, 0.0001);

  input.windowContainerHeight = 400.0;
  Virtualizer::update(&container, input);
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const Element& element = container.revision.elements[index];
    CHECK_NEAR(element.height, 200.0, 0.0001);
    CHECK_NEAR(element.offsetY, (index % 2) * 200.0, 0.0001);
  }
}

TEST(borrowed_sticky_indices_match_copied_ones) {
  std::vector<std::string> keys = keysFor(50);
  std::vector<std::size_t> sticky = {0, 10, 20};

  Container copied;
  FrameInput copiedInput = inputFor(keys, 0.0);
  copiedInput.stickyIndices = sticky;
  Virtualizer::update(&copied, copiedInput);

  Container borrowed;
  FrameInput borrowedInput = inputFor(keys, 0.0);
  borrowedInput.stickyIndicesRef = &sticky;
  Virtualizer::update(&borrowed, borrowedInput);
  CHECK(copied.stickyIndices == borrowed.stickyIndices);

  // A new list replaces the old one, and an empty one clears it.
  std::vector<std::size_t> next = {5};
  borrowedInput.stickyIndicesRef = &next;
  Virtualizer::update(&borrowed, borrowedInput);
  CHECK(borrowed.stickyIndices == next);
  std::vector<std::size_t> none;
  borrowedInput.stickyIndicesRef = &none;
  Virtualizer::update(&borrowed, borrowedInput);
  CHECK(borrowed.stickyIndices.empty());
}

TEST(unchanged_anchor_ignore_keys_keep_the_set_the_core_already_has) {
  std::vector<std::string> keys = keysFor(30);
  std::vector<std::string> ignored = {"k0", "k1"};
  Container container;
  FrameInput input = inputFor(keys, 0.0);
  input.nonAnchorableKeysRef = &ignored;
  Virtualizer::update(&container, input);
  CHECK(!container.isAnchorable("k0"));
  CHECK(!container.isAnchorable("k1"));
  CHECK(container.isAnchorable("k2"));

  // The host vouches the keys are the ones it sent before, so the set stays as it is.
  input.nonAnchorableKeysUnchanged = true;
  Virtualizer::update(&container, input);
  CHECK(!container.isAnchorable("k0"));
  CHECK_EQ(container.nonAnchorableKeys.size(), static_cast<std::size_t>(2));

  // New keys without that promise replace the set.
  std::vector<std::string> nextIgnored = {"k2"};
  input.nonAnchorableKeysRef = &nextIgnored;
  input.nonAnchorableKeysUnchanged = false;
  Virtualizer::update(&container, input);
  CHECK(container.isAnchorable("k0"));
  CHECK(!container.isAnchorable("k2"));
}
