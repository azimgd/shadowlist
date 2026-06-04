/*
 * Emscripten/embind binding around the platform-agnostic shadowlist-core.
 *
 * It mirrors what the React Native Fabric integration
 * (ShadowlistViewShadowNode) does, but exposes a flat API to JavaScript so the
 * react-dom layer can drive the exact same virtualization algorithm:
 *
 *   1. update(...)            -> reconcile keys, measure, resolve scroll
 *   2. read getElementAtIndex -> position the rendered DOM nodes
 *   3. updateElementAtIndex   -> feed measured DOM sizes back
 *   4. recomputeTotalSize     -> refresh content size once per batch
 *   5. resolveStateUpdate     -> learn the content size / scroll correction
 *
 * One ShadowlistCore instance owns one Container + Virtualizer, matching the
 * "one core per list" lifetime that the Fabric ShadowNode family enforces.
 */
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>
#include <vector>

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/Element.hpp>
#include <shadowlist-core/Constants.hpp>

using namespace emscripten;
using namespace azimgd::shadowlist;

namespace {

int toSignedIndex(std::size_t index) {
  return index == UNDEFINED_INDEX ? -1 : static_cast<int>(index);
}

} // namespace

class ShadowlistCore {
public:
  ShadowlistCore() = default;

  /*
   * Single per-frame entry point. Marshals JS props into a FrameInput and runs
   * the virtualizer (reconcile -> measure -> resolve scroll -> dispatch).
   */
  void update(
    val keysVal,
    double containerOffsetX,
    double containerOffsetY,
    double windowContainerWidth,
    double windowContainerHeight,
    double headerSize,
    double footerSize,
    bool inverted,
    bool horizontal,
    int columns,
    double estimatedWidth,
    double estimatedHeight) {
    FrameInput input;
    input.keys = vecFromJSArray<std::string>(keysVal);
    input.containerOffsetX = containerOffsetX;
    input.containerOffsetY = containerOffsetY;
    input.windowContainerWidth = windowContainerWidth;
    input.windowContainerHeight = windowContainerHeight;
    input.headerSize = headerSize;
    input.footerSize = footerSize;
    input.inverted = inverted;
    input.horizontal = horizontal;
    input.columns = columns > 0 ? static_cast<std::size_t>(columns) : 1;
    input.estimatedElementSize = {estimatedWidth, estimatedHeight};

    virtualizer_.update(&container_, input);
  }

  /*
   * Feed a natively measured element size back into the virtualizer. Guarded
   * against a stale index outrunning the reconciled element count.
   */
  void updateElementAtIndex(int index, double width, double height) {
    if (index < 0 || static_cast<std::size_t>(index) >= container_.getElementsSize()) {
      return;
    }
    virtualizer_.updateElementAtIndex(&container_, static_cast<std::size_t>(index), {width, height});
  }

  /*
   * Refresh the total content size once after a batch of measurement feedback.
   */
  void recomputeTotalSize() {
    Virtualizer::recomputeTotalSize(&container_);
  }

  int getElementsSize() const {
    return static_cast<int>(container_.getElementsSize());
  }

  /*
   * Layout (offset + size) of a single element, used to position the DOM node.
   */
  val getElementAtIndex(int index) const {
    val result = val::object();
    if (index < 0 || static_cast<std::size_t>(index) >= container_.getElementsSize()) {
      return result;
    }
    const Element element = container_.getElementAtIndex(static_cast<std::size_t>(index));
    result.set("index", static_cast<int>(element.index));
    result.set("key", element.key);
    result.set("offsetX", element.offsetX);
    result.set("offsetY", element.offsetY);
    result.set("width", element.width);
    result.set("height", element.height);
    result.set("measured", element.measured);
    return result;
  }

  /*
   * Current visible index range (inverted lists report start > end).
   */
  val getVisibleIndices() const {
    auto visibleIndices = container_.getVisibleIndices();
    val result = val::object();
    result.set("visibleStartIndex", toSignedIndex(visibleIndices.first));
    result.set("visibleEndIndex", toSignedIndex(visibleIndices.second));
    return result;
  }

  /*
   * Resolve the frame into the values to publish to the scroll view. prev* are
   * what the DOM currently holds (its scroll offset and last content size).
   */
  val resolveStateUpdate(
    double prevContainerOffsetX,
    double prevContainerOffsetY,
    double prevTotalContainerWidth,
    double prevTotalContainerHeight) const {
    auto stateUpdate = container_.resolveStateUpdate(
      prevContainerOffsetX,
      prevContainerOffsetY,
      prevTotalContainerWidth,
      prevTotalContainerHeight);
    val result = val::object();
    result.set("changed", stateUpdate.changed);
    result.set("applyContainerOffset", stateUpdate.applyContainerOffset);
    result.set("containerOffsetX", stateUpdate.containerOffsetX);
    result.set("containerOffsetY", stateUpdate.containerOffsetY);
    result.set("totalContainerWidth", stateUpdate.totalContainerWidth);
    result.set("totalContainerHeight", stateUpdate.totalContainerHeight);
    return result;
  }

  double getFooterOffset(double footerSize) const {
    return container_.getFooterOffset(footerSize);
  }

  /* Imperative scroll command (fires once per call via the nonce). */
  void scrollToIndex(int index) {
    if (index < 0) {
      return;
    }
    container_.scrollToIndex(static_cast<std::size_t>(index));
  }

  /*
   * Resolve a scrollToIndex from an imperative command (index + monotonic nonce)
   * and a declarative prop, matching the Fabric command/prop precedence.
   */
  void requestScrollToIndex(double commandIndex, double commandNonce, int propIndex) {
    container_.requestScrollToIndex(commandIndex, commandNonce, propIndex);
  }

  void toggleEndReached(bool enabled) {
    container_.toggleEndReached(enabled);
  }

  void toggleStartReached(bool enabled) {
    container_.toggleStartReached(enabled);
  }

  void setOnEndReached(val callback) {
    onEndReached_ = callback;
    container_.onEndReachedCallback = [this]() {
      if (!onEndReached_.isUndefined() && !onEndReached_.isNull()) {
        onEndReached_();
      }
    };
  }

  void setOnStartReached(val callback) {
    onStartReached_ = callback;
    container_.onStartReachedCallback = [this]() {
      if (!onStartReached_.isUndefined() && !onStartReached_.isNull()) {
        onStartReached_();
      }
    };
  }

  void setOnVisibleIndicesChange(val callback) {
    onVisibleIndicesChange_ = callback;
    container_.onVisibleIndicesChangeCallback = [this](std::size_t startIndex, std::size_t endIndex) {
      if (!onVisibleIndicesChange_.isUndefined() && !onVisibleIndicesChange_.isNull()) {
        onVisibleIndicesChange_(toSignedIndex(startIndex), toSignedIndex(endIndex));
      }
    };
  }

  void setOnScroll(val callback) {
    onScroll_ = callback;
    container_.onScrollCallback = [this](double containerOffsetX, double containerOffsetY) {
      if (!onScroll_.isUndefined() && !onScroll_.isNull()) {
        onScroll_(containerOffsetX, containerOffsetY);
      }
    };
  }

private:
  Container container_;
  Virtualizer virtualizer_;

  val onEndReached_ = val::undefined();
  val onStartReached_ = val::undefined();
  val onVisibleIndicesChange_ = val::undefined();
  val onScroll_ = val::undefined();
};

EMSCRIPTEN_BINDINGS(shadowlist_core) {
  class_<ShadowlistCore>("ShadowlistCore")
    .constructor<>()
    .function("update", &ShadowlistCore::update)
    .function("updateElementAtIndex", &ShadowlistCore::updateElementAtIndex)
    .function("recomputeTotalSize", &ShadowlistCore::recomputeTotalSize)
    .function("getElementsSize", &ShadowlistCore::getElementsSize)
    .function("getElementAtIndex", &ShadowlistCore::getElementAtIndex)
    .function("getVisibleIndices", &ShadowlistCore::getVisibleIndices)
    .function("resolveStateUpdate", &ShadowlistCore::resolveStateUpdate)
    .function("getFooterOffset", &ShadowlistCore::getFooterOffset)
    .function("scrollToIndex", &ShadowlistCore::scrollToIndex)
    .function("requestScrollToIndex", &ShadowlistCore::requestScrollToIndex)
    .function("toggleEndReached", &ShadowlistCore::toggleEndReached)
    .function("toggleStartReached", &ShadowlistCore::toggleStartReached)
    .function("setOnEndReached", &ShadowlistCore::setOnEndReached)
    .function("setOnStartReached", &ShadowlistCore::setOnStartReached)
    .function("setOnVisibleIndicesChange", &ShadowlistCore::setOnVisibleIndicesChange)
    .function("setOnScroll", &ShadowlistCore::setOnScroll);
}
