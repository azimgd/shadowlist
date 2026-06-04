#pragma once

#include <functional>
#include <chrono>
#include <memory>
#include <mutex>
#include <shadowlist-core/Revision.hpp>

namespace azimgd::shadowlist {

class Observer;

static constexpr std::size_t RevisionCountFirst = 0;

static constexpr std::size_t RevisionStatusIdle = 0;
static constexpr std::size_t RevisionStatusPending = 1;

/*
 * Result of resolving a frame into the values an integration should publish to
 * its platform scroll view. Keeps the "what changed / should we move the view"
 * decision in the core so every integration applies it the same way.
 */
struct ContainerStateUpdate {
  /*
   * Whether anything changed and the integration should publish new state
   */
  bool changed = false;

  /*
   * Whether the integration should move the scroll view to (containerOffsetX,
   * containerOffsetY). False means the offset is the view's own position and must
   * be left alone (so we never fight the user's scrolling).
   */
  bool applyContainerOffset = false;

  double containerOffsetX = 0.0;
  double containerOffsetY = 0.0;
  double totalContainerWidth = 0.0;
  double totalContainerHeight = 0.0;
};

class Container {
public:
  /*
   * Estimated element size for unmeasured elements (width, height)
   */
  std::pair<double, double> estimatedElementSize = {120.0, 120.0};

  /*
   * Callback to be executed when scrolled near the end of the list
   */
  std::function<void()> onEndReachedCallback;

  /*
   * Callback to be executed when scrolled near the start of the list
   */
  std::function<void()> onStartReachedCallback;

  /*
   * Callback to be executed when the visible element range changes
   * Arguments are (startIndex, endIndex) of the visible range
   */
  std::function<void(std::size_t, std::size_t)> onVisibleIndicesChangeCallback;

  /*
   * Callback to be executed when the scroll offset changes
   * Arguments are (containerOffsetX, containerOffsetY)
   */
  std::function<void(double, double)> onScrollCallback;

  /*
   * Enable/disable end reached callback
   */
  bool endReachedEnabled = true;

  /*
   * Enable/disable start reached callback
   */
  bool startReachedEnabled = true;

  /*
   * Current measurement revision
   */
  Revision revision = {};

  /*
   * Current active revision index
   */
  std::size_t revisionCount = RevisionCountFirst;

  /*
   * Current active revision status
   */
  std::size_t revisionStatus = RevisionStatusIdle;

  /*
   * Default / Inverted order of the list
   */
  bool inverted = false;
  
  /*
   * Horizontal / Vertical position of the list
   */
  bool horizontal = false;

  /*
   * Number of columns for multi-column layout
   */
  std::size_t columns = 1;

  /*
   * Size of the header (and empty) template along the scroll axis
   * Elements are positioned after the header and the total size includes it
   */
  double headerSize = 0.0;

  /*
   * Size of the footer template along the scroll axis, included in the total size
   */
  double footerSize = 0.0;

  /*
   * Pending scrollToIndex target, or UNDEFINED_INDEX when inactive
   */
  std::size_t scrollToIndexTarget = UNDEFINED_INDEX;

  /*
   * Whether an inverted list has settled at the bottom. While false the list
   * sticks to the bottom (initial render / empty -> populated); once the view
   * actually reaches the bottom the maintain-visible-content-position anchor takes over
   */
  bool invertedInitialized = false;

  /*
   * Set by the scroll resolution each frame: true when the core wants the
   * integration to apply containerOffset to the scroll view (scrollToIndex,
   * inverted bottom anchor, or a maintain-visible-content-position shift). When
   * false the integration must leave the scroll position to the user.
   */
  bool containerOffsetCorrected = false;

  /*
   * A scroll target the core actively drives the view toward until the view
   * reports it has arrived. This keeps a correction alive across the redundant
   * re-commits that the visible-indices event triggers, so a stale offset on a
   * racing frame cannot cancel it.
   */
  double pendingScrollOffset = 0.0;
  bool pendingScroll = false;

  /*
   * When the pending correction is a maintain-visible-content-position shift
   * (e.g. prepend) the target is the anchor element rather than a fixed offset,
   * so it tracks the anchor as nearby elements are measured and resized.
   */
  std::string pendingAnchorKey = "";
  double pendingAnchorDelta = 0.0;
  bool pendingAnchorActive = false;

  /*
   * The element at the viewport edge this frame and how far we are scrolled into
   * it. Used to keep the visible content fixed while off-screen elements are
   * measured (their real size differs from the estimate).
   */
  std::string anchorKey = "";
  double anchorDelta = 0.0;

  /*
   * Serializes access to a single container. An integration may drive the update
   * pass and the layout/measurement-feedback pass from different threads that can
   * overlap, so every entry point that mutates or reads the revision takes this.
   * Recursive because a locked entry point may re-enter another one.
   */
  std::recursive_mutex coreMutex;

  void startRevision();
  void endRevision();

  void addElementAtIndex(std::size_t index, Element nextElement);
  void removeElementAtIndex(std::size_t index);
  const Element getElementAtIndex(std::size_t index) const;
  bool getElementVisible(std::size_t index) const;

  /*
   * Offset/size getters and setters are orientation aware: horizontal reads/writes
   * offsetX and width, vertical reads/writes offsetY and height.
   */
  double getElementOffset(std::size_t index) const;
  double getElementSize(std::size_t index) const;
  void setElementOffset(std::size_t index, double offset);
  double getContainerOffset() const;
  double getWindowContainerSize() const;

  std::size_t getElementsSize() const;

  void setWindowContainerHeight(double height);
  void setWindowContainerWidth(double width);
  void setContainerOffsetY(double offsetY);
  void setContainerOffsetX(double offsetX);

  std::size_t getMeasurementElementStartIndex() const;
  std::size_t getMeasurementElementEndIndex() const;

  std::string getDebugRepresentation() const;
  std::vector<Element> getVisibleElements() const;

  /*
   * Visible index range, or (UNDEFINED_INDEX, UNDEFINED_INDEX) if uninitialized.
   */
  std::pair<std::size_t, std::size_t> getVisibleIndices() const;

  void setEndReachedEnabled(bool enabled);
  void setStartReachedEnabled(bool enabled);

  /*
   * Request scrolling so the element at the given index sits at the start of the
   * viewport. The request is resolved on the next measurement.
   */
  void scrollToIndex(std::size_t index);

  /*
   * Resolve a scrollToIndex request from an imperative command and a declarative
   * prop index. The imperative command fires once per invocation, tracked by a
   * monotonic nonce so repeating the same index re-scrolls; the declarative prop
   * fires whenever its value changes. Both use a negative index to mean
   * "inactive", and the imperative command takes precedence.
   */
  void requestScrollToIndex(double commandIndex, double commandNonce, int propIndex);

  /*
   * Resolve the current frame into the values an integration should publish to its
   * platform scroll view. prev* are the values the platform currently holds (the
   * view's reported scroll offset and the last published content size).
   */
  ContainerStateUpdate resolveStateUpdate(
    double prevContainerOffsetX,
    double prevContainerOffsetY,
    double prevTotalContainerWidth,
    double prevTotalContainerHeight) const;

  /*
   * Offset of the footer along the scroll axis (placed after the content)
   */
  double getFooterOffset(double footerSize) const;

  /*
   * Find the index of the element with the given key, or UNDEFINED_INDEX if absent
   */
  std::size_t findElementIndexByKey(const std::string& key) const;

  /*
   * Fire the visible-indices-change and scroll callbacks if their values changed
   * since the last revision (deduplication lives here so integrations don't repeat it)
   */
  void dispatchObservers();

  /*
   * Observer is optional; pass nullptr to remove it.
   */
  void setObserver(Observer* observer);
  Observer* getObserver() const;

private:
  /*
   * Observer for revision changes
   */
  Observer* observer = nullptr;

  /*
   * Previously dispatched visible range, used to deduplicate onVisibleIndicesChange
   */
  std::size_t prevVisibleStartIndex = UNDEFINED_INDEX;
  std::size_t prevVisibleEndIndex = UNDEFINED_INDEX;

  /*
   * Previously dispatched scroll offset, used to deduplicate onScroll
   */
  double prevContainerOffsetX = 0.0;
  double prevContainerOffsetY = 0.0;
  bool prevContainerOffsetValid = false;

  /*
   * Last imperative scrollToIndex nonce we acted on, so the command fires once
   * per invocation (a repeated index still re-scrolls because the nonce changes)
   */
  double prevScrollToIndexNonce = 0.0;

  /*
   * Last declarative containerOffsetIndex prop we acted on, so the prop fires
   * only when its value changes
   */
  int prevScrollToIndexProp = -1;
};

}
