#pragma once

#include <functional>
#include <vector>
#include <chrono>
#include <memory>

namespace azimgd::shadowlist {

class Container;
class Revision;

/*
 * Observer class to track revision change operations with throttling
 */
class Observer {
public:
  /*
   * Gets called with Container when changes happen
   */
  using RevisionCallback = std::function<void(Container&)>;

  /*
   * Creates a new Observer
   */
  Observer(Container& container, std::size_t throttleMs = 32);

  /*
   * Cleans up the Observer
   */
  ~Observer();

  /*
   * Add a callback function
   * Returns a subscription id
   */
  std::size_t subscribe(RevisionCallback callback);

  /*
   * Remove a callback using its subscription id
   */
  void unsubscribe(std::size_t subscriptionId);

  /*
   * Called by Container when endRevision happens
   * Only triggers callbacks if enough time has passed
   */
  void notifyEndRevision();

  /*
   * Run pending callbacks right now (skips throttle)
   */
  void flush();

  /*
   * Get scroll speed in pixels per second
   * Positive = scrolling down/right, Negative = scrolling up/left
   */
  double getScrollVelocity() const;

private:
  /*
   * Reference to the Container being observed
   */
  Container& container;

  /*
   * Array of callbacks
   */
  std::vector<std::pair<std::size_t, RevisionCallback>> callbacks;

  /*
   * Next subscription id
   */
  std::size_t nextSubscriptionId;

  /*
   * Throttle limit in milliseconds
   */
  std::size_t throttleMs;

  /*
   * Timestamp of previous callback execution
   */
  std::chrono::milliseconds prevDispatchTimestamp;

  /*
   * Previous measurement start index
   */
  std::size_t prevMeasurementElementStartIndex;

  /*
   * Previous measurement end index
   */
  std::size_t prevMeasurementElementEndIndex;

  /*
   * Previous measurement container height
   */
  double prevMeasurementElementTotalHeight;

  /*
   * Previous measurement container width
   */
  double prevMeasurementElementTotalWidth;

  /*
   * Previous container offset
   */
  double prevContainerOffset;

  /*
   * Current scroll velocity in pixels per second
   * Positive = scrolling down/right, Negative = scrolling up/left
   */
  double scrollVelocity;

  /*
   * Indicates if there's a pending dispatch
   */
  bool pendingDispatch;

  /*
   * Execute callbacks
   */
  void executeCallbacks();

  /*
   * Check if visible indices have changed
   * @return True if indices have changed since last dispatch
   */
  bool hasIndicesChanged() const;
};

}

