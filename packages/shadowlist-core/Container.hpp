#ifndef Container_hpp
#define Container_hpp

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
  Revision nextRevision = {};

  /*
   * Current active revision index
   */
  std::size_t nextRevisionCount = RevisionCountFirst;

  /*
   * Current active revision status
   */
  std::size_t nextRevisionStatus = RevisionStatusIdle;

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
  size_t columns = 1;

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
   * Serializes core access to a single container. Fabric runs the commit phase
   * (Virtualizer::update via the descriptor) and the layout phase (ShadowNode
   * layout / measurement feedback) lock free and can overlap on background
   * threads, so every entry point that mutates or reads the revision takes this.
   * Recursive because layout() locks it and then calls replaceChild().
   */
  std::recursive_mutex coreMutex;

  /**
   * Start a new revision cycle for measurements and state updates
   */
  void startRevision();

  /**
   * End the current revision cycle
   */
  void endRevision();

  /**
   * Add an element at the specified index
   */
  void addElementAtIndex(std::size_t index, Element nextElement);

  /**
   * Remove an element at the specified index
   */
  void removeElementAtIndex(std::size_t index);

  /**
   * Get an element at the specified index
   */
  const Element getElementAtIndex(std::size_t index) const;

  /**
   * Check if element at index is visible in the current revision
   */
  bool getElementVisible(std::size_t index) const;

  /**
   * Get the offset for an element based on orientation (horizontal returns offsetX, vertical returns offsetY)
   */
  double getElementOffset(std::size_t index) const;

  /**
   * Get the size for an element based on orientation (horizontal returns width, vertical returns height)
   */
  double getElementSize(std::size_t index) const;

  /**
   * Set the offset for an element based on orientation (horizontal sets offsetX, vertical sets offsetY)
   */
  void setElementOffset(std::size_t index, double offset);

  /**
   * Get the container scroll offset based on orientation (horizontal returns containerOffsetX, vertical returns containerOffsetY)
   */
  double getContainerOffset() const;

  /**
   * Get the window container size based on orientation (horizontal returns windowContainerWidth, vertical returns windowContainerHeight)
   */
  double getWindowContainerSize() const;

  /**
   * Resize the elements vector from tail
   */
  void resizeElementsTail(std::size_t size);

  /**
   * Get the current number of elements
   */
  std::size_t getElementsSize() const;

  /**
   * Set the window container height
   */
  void setWindowContainerHeight(double height);

  /**
   * Set the window container width
   */
  void setWindowContainerWidth(double width);

  /**
   * Set the container Y offset
   */
  void setContainerOffsetY(double offsetY);

  /**
   * Set the container X offset
   */
  void setContainerOffsetX(double offsetX);

  /**
   * Get the measurement start index
   */
  std::size_t getMeasurementElementStartIndex() const;

  /**
   * Get the measurement end index
   */
  std::size_t getMeasurementElementEndIndex() const;

  /**
   * Get debug representation as JSON string
   */
  std::string getDebugRepresentation() const;

  /**
   * Get all visible elements
   * Returns a vector of elements that are currently visible in the viewport
   */
  std::vector<Element> getVisibleElements() const;

  /**
   * Get visible element index range
   * Returns a pair containing (startIndex, endIndex), or (-1, -1) if uninitialized
   */
  std::pair<std::size_t, std::size_t> getVisibleIndices() const;

  /**
   * Enable or disable the end reached callback
   */
  void toggleEndReached(bool enabled);

  /**
   * Enable or disable the start reached callback
   */
  void toggleStartReached(bool enabled);

  /**
   * Request scrolling so the element at the given index is at the start of the viewport
   * The request is resolved on the next measurement
   */
  void scrollToIndex(std::size_t index);

  /**
   * Resolve a scrollToIndex request from an imperative command and a declarative
   * prop index. The imperative command fires once per invocation, tracked by a
   * monotonic nonce so repeating the same index re-scrolls; the declarative prop
   * fires whenever its value changes. Both use a negative index to mean
   * "inactive", and the imperative command takes precedence.
   */
  void requestScrollToIndex(double commandIndex, double commandNonce, int propIndex);

  /**
   * Resolve the current frame into the values an integration should publish to its
   * platform scroll view. prev* are the values the platform currently holds (the
   * view's reported scroll offset and the last published content size).
   */
  ContainerStateUpdate resolveStateUpdate(
    double prevContainerOffsetX,
    double prevContainerOffsetY,
    double prevTotalContainerWidth,
    double prevTotalContainerHeight) const;

  /**
   * Offset of the footer along the scroll axis (placed after the content)
   */
  double getFooterOffset(double footerSize) const;

  /**
   * Find the index of the element with the given key, or UNDEFINED_INDEX if absent
   */
  std::size_t findElementIndexByKey(const std::string& key) const;

  /**
   * Fire the visible-indices-change and scroll callbacks if their values changed
   * since the last revision (deduplication lives here so integrations don't repeat it)
   */
  void dispatchObservers();

  /**
   * Set observer for this container
   * @param observer Pointer to observer instance (can be nullptr to remove)
   */
  void setObserver(Observer* observer);

  /**
   * Get the current observer
   * @return Pointer to observer instance (may be nullptr)
   */
  Observer* getObserver() const;

private:
  /**
   * Observer for revision changes
   */
  Observer* observer = nullptr;

  /**
   * Previously dispatched visible range, used to deduplicate onVisibleIndicesChange
   */
  std::size_t prevVisibleStartIndex = UNDEFINED_INDEX;
  std::size_t prevVisibleEndIndex = UNDEFINED_INDEX;

  /**
   * Previously dispatched scroll offset, used to deduplicate onScroll
   */
  double prevContainerOffsetX = 0.0;
  double prevContainerOffsetY = 0.0;
  bool prevContainerOffsetValid = false;

  /**
   * Last imperative scrollToIndex nonce we acted on, so the command fires once
   * per invocation (a repeated index still re-scrolls because the nonce changes)
   */
  double prevScrollToIndexNonce = 0.0;

  /**
   * Last declarative containerOffsetIndex prop we acted on, so the prop fires
   * only when its value changes
   */
  int prevScrollToIndexProp = -1;
};

}
#endif
