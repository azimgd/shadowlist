#pragma once

// Driver that exercises the core one frame at a time: update, feed measured
// sizes, recompute total, then apply any requested scroll offset.

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <cmath>

namespace slt {

using azimgd::shadowlist::Container;
using azimgd::shadowlist::FrameInput;
using azimgd::shadowlist::Size;
using azimgd::shadowlist::Virtualizer;
using azimgd::shadowlist::UNDEFINED_INDEX;

// Build ["k0", "k1", ... "k(n-1)"] (prefix overridable).
inline std::vector<std::string> makeKeys(std::size_t count, const std::string& prefix = "k") {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    keys.push_back(prefix + std::to_string(i));
  }
  return keys;
}

struct Sim {
  Container container;
  Virtualizer virtualizer;

  // List configuration.
  std::vector<std::string> keys;
  bool inverted = false;
  bool horizontal = false;
  std::size_t columns = 1;
  double headerSize = 0.0;
  double footerSize = 0.0;
  bool stickyHeader = false;
  bool stickyFooter = false;
  std::vector<std::size_t> stickyIndices;
  double startReachedThreshold = 1.0;
  double endReachedThreshold = 1.0;
  double viewablePercentThreshold = 0.0;
  double winW = 400.0;
  double winH = 600.0;
  Size estimated = {100.0, 100.0};

  // Real measured size of a row; defaults to the estimate when unset.
  std::function<Size(const std::string& key)> sizeOfKey;

  // Current scroll offset.
  double offsetX = 0.0;
  double offsetY = 0.0;

  // Flags the NEXT frame as user-initiated; consumed and reset in frame().
  bool nextUserScrolled = false;

  // Last published content size, fed back into resolveStateUpdate.
  double prevTotalW = 0.0;
  double prevTotalH = 0.0;

  // Captured observer callbacks.
  int endReached = 0;
  int startReached = 0;
  std::vector<std::pair<std::size_t, std::size_t>> visibleChanges;
  std::vector<std::pair<std::size_t, std::size_t>> viewableChanges;
  std::vector<std::pair<double, double>> scrolls;

  Sim() {
    container.onEndReachedCallback = [this]() { ++endReached; };
    container.onStartReachedCallback = [this]() { ++startReached; };
    container.onVisibleIndicesChangeCallback = [this](std::size_t s, std::size_t e) {
      visibleChanges.push_back({s, e});
    };
    container.onViewableIndicesChangeCallback = [this](std::size_t s, std::size_t e) {
      viewableChanges.push_back({s, e});
    };
    container.onScrollCallback = [this](double x, double y) { scrolls.push_back({x, y}); };
  }

  // Viewable window, normalised to ascending [lo, hi].
  bool viewableRange(std::size_t& lo, std::size_t& hi) {
    auto viewable = container.getViewableIndices();
    if (viewable.first == UNDEFINED_INDEX || viewable.second == UNDEFINED_INDEX) {
      return false;
    }
    lo = inverted ? viewable.second : viewable.first;
    hi = inverted ? viewable.first : viewable.second;
    return true;
  }

  void setKeys(std::vector<std::string> next) { keys = std::move(next); }

  // Visible window, normalised to ascending [lo, hi].
  bool visibleRange(std::size_t& lo, std::size_t& hi) {
    auto visible = container.getVisibleIndices();
    if (visible.first == UNDEFINED_INDEX || visible.second == UNDEFINED_INDEX) {
      return false;
    }
    lo = inverted ? visible.second : visible.first;
    hi = inverted ? visible.first : visible.second;
    return true;
  }

  double offsetAxis() const { return horizontal ? offsetX : offsetY; }
  double totalAxis() { return horizontal ? container.revision.totalContainerWidth : container.revision.totalContainerHeight; }
  double elementOffset(std::size_t index) { return container.getElementOffset(index); }
  std::size_t indexOfKey(const std::string& key) { return container.findElementIndexByKey(key); }

  // Feed the measured size of every visible row, then refresh the total once.
  void feedMeasuredSizes() {
    std::size_t lo = 0;
    std::size_t hi = 0;
    if (!visibleRange(lo, hi)) {
      return;
    }
    for (std::size_t index = lo; index <= hi && index < container.getElementsSize(); ++index) {
      auto element = container.getElementAtIndex(index);
      Size size = sizeOfKey ? sizeOfKey(element.key) : estimated;
      Virtualizer::updateElementAtIndex(&container, index, size);
    }
    Virtualizer::recomputeTotalSize(&container);
  }

  // Build the per-frame input from the current configuration (userScrolled is
  // set by the caller).
  FrameInput makeInput() const {
    FrameInput input;
    input.keys = keys;
    input.containerOffsetX = offsetX;
    input.containerOffsetY = offsetY;
    input.windowContainerWidth = winW;
    input.windowContainerHeight = winH;
    input.inverted = inverted;
    input.horizontal = horizontal;
    input.columns = columns;
    input.headerSize = headerSize;
    input.footerSize = footerSize;
    input.stickyHeader = stickyHeader;
    input.stickyFooter = stickyFooter;
    input.stickyIndices = stickyIndices;
    input.startReachedThreshold = startReachedThreshold;
    input.endReachedThreshold = endReachedThreshold;
    input.viewablePercentThreshold = viewablePercentThreshold;
    input.estimatedElementSize = {estimated.width, estimated.height};
    return input;
  }

  // Feed measured sizes, then apply any requested offset. Returns whether it moved.
  bool publish() {
    feedMeasuredSizes();

    auto update = container.resolveStateUpdate(offsetX, offsetY, prevTotalW, prevTotalH);
    bool moved = update.applyContainerOffset;
    if (moved) {
      offsetX = update.containerOffsetX;
      offsetY = update.containerOffsetY;
    }
    prevTotalW = update.totalContainerWidth;
    prevTotalH = update.totalContainerHeight;
    return moved;
  }

  // Run one frame. Returns true if the core moved the scroll offset this frame.
  bool frame() {
    FrameInput input = makeInput();
    input.userScrolled = nextUserScrolled;
    nextUserScrolled = false;

    virtualizer.update(&container, input);
    return publish();
  }

  // Run update() N times with the same input, then publish once. Models a commit
  // that adopts the node multiple times before applying the offset. Returns moved.
  bool commit(int adopts, bool userScrolled) {
    FrameInput input = makeInput();
    input.userScrolled = userScrolled;
    for (int i = 0; i < adopts; ++i) {
      virtualizer.update(&container, input);
    }
    return publish();
  }

  // Run frames until the offset stops moving (or maxFrames is reached).
  void settle(int maxFrames = 40) {
    for (int i = 0; i < maxFrames; ++i) {
      double beforeX = offsetX;
      double beforeY = offsetY;
      bool moved = frame();
      if (!moved && std::fabs(offsetX - beforeX) < 0.5 && std::fabs(offsetY - beforeY) < 0.5) {
        break;
      }
    }
  }

  // Set the scroll offset (as if the user dragged) and run to steady state.
  void scrollTo(double y) {
    offsetY = y;
    settle();
  }

  // Request a core-driven scroll to the end and run to steady state.
  void scrollToEnd() {
    container.scrollToEnd();
    settle();
  }

  // Run one user-initiated scroll frame to offset y.
  bool userScrollTo(double y) {
    offsetY = y;
    nextUserScrolled = true;
    return frame();
  }
};

}  // namespace slt

