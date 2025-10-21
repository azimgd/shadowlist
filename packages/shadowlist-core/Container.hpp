#ifndef Container_hpp
#define Container_hpp

#include <functional>
#include <chrono>
#include <memory>
#include <shadowlist-core/Revision.hpp>

namespace azimgd::shadowlist {

class Observer;

static constexpr std::size_t RevisionCountFirst = 0;

static constexpr std::size_t RevisionStatusIdle = 0;
static constexpr std::size_t RevisionStatusPending = 1;

class Container {
public:
  /*
   * Callback to be executed for each element measurement
   */
  std::function<std::pair<double, double>(std::size_t)> measurementCallback;

  /*
   * Callback to be executed for offset adjustment
   */
  std::function<void(Revision)> sizeAdjustmentCallback;

  /*
   * Callback to be executed for size adjustment
   */
  bool sizeAdjustmentCallbackCompleted = false;

  /*
   * Callback to be executed for the initial offset adjustment
   */
  std::function<void(Revision)> offsetInitAdjustmentCallback;

  /*
   * Callback to be executed for size adjustment
   */
  bool offsetInitAdjustmentCallbackCompleted = false;

  /*
   * Callback to be executed for the initial offset adjustment
   */
  std::function<void(Revision)> offsetMvcpAdjustmentCallback;

  /*
   * Callback to be executed for size adjustment
   */
  bool offsetMvcpAdjustmentCallbackCompleted = true;

  /*
   * Callback to be executed when scrolled near the end of the list
   */
  std::function<void()> onEndReachedCallback;

  /*
   * Callback to be executed when scrolled near the start of the list
   */
  std::function<void()> onStartReachedCallback;

  /*
   * Enable/disable end reached callback
   */
  bool endReachedEnabled = true;

  /*
   * Enable/disable start reached callback
   */
  bool startReachedEnabled = true;

  /*
   * Previous measurement revision
   */
  Revision prevRevision = {};

  /*
   * Previous measurement revision
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
   * Timestamp of next revision
   */
  std::chrono::milliseconds nextRevisionTimestamp = std::chrono::milliseconds(0);

  /*
   * Timestamp of previous revision
   */
  std::chrono::milliseconds prevRevisionTimestamp = std::chrono::milliseconds(0);

  /*
   * Default / Inverted order of the list
   */
  bool inverted = false;
  
  /*
   * Horizontal / Vertical position of the list
   */
  bool horizontal = false;

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
   * Resize the elements vector from head
   */
  void resizeElementsHead(std::size_t size);

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
  std::string getDebugRepresentation(const RevisionDebugRepresentationMetadata& metadata) const;

  /**
   * Set offset init adjustment completion state
   */
  void setOffsetInitAdjustmentCompleted(bool completed);

  /**
   * Set offset mvcp adjustment completion state
   */
  void setOffsetMvcpAdjustmentCompleted(bool completed);

  /**
   * Set size adjustment completion state
   */
  void setSizeAdjustmentCallbackCompleted(bool completed);

  /**
   * Get metadata for current revision
   */
  RevisionDebugRepresentationMetadata getMetadata() const;

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
};

}
#endif
