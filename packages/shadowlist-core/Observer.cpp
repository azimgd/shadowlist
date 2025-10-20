#include <shadowlist-core/Observer.hpp>
#include <shadowlist-core/Container.hpp>
#include <cmath>

namespace azimgd::shadowlist {

Observer::Observer(Container& container, std::size_t throttleMs)
  : container(container),
    nextSubscriptionId(0),
    throttleMs(throttleMs),
    prevNotificationTimestamp(std::chrono::milliseconds(0)),
    prevMeasurementElementStartIndex(static_cast<std::size_t>(-1)),
    prevMeasurementElementEndIndex(static_cast<std::size_t>(-1)),
    prevMeasurementContainerHeight(0.0),
    prevMeasurementContainerWidth(0.0),
    prevContainerOffset(0.0),
    scrollVelocity(0.0),
    pendingNotification(false) {
}

Observer::~Observer() {
  callbacks.clear();
}

std::size_t Observer::subscribe(RevisionCallback callback) {
  std::size_t subscriptionId = nextSubscriptionId++;
  callbacks.push_back({subscriptionId, callback});
  return subscriptionId;
}

void Observer::unsubscribe(std::size_t subscriptionId) {
  callbacks.erase(
    std::remove_if(callbacks.begin(), callbacks.end(),
      [subscriptionId](const std::pair<std::size_t, RevisionCallback>& pair) {
        return pair.first == subscriptionId;
      }),
    callbacks.end()
  );
}

void Observer::notifyEndRevision() {
  if (!hasIndicesChanged()) {
    return;
  }

  auto nextNotificationTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
  );
  auto notificationTimestampDiff = nextNotificationTimestamp - prevNotificationTimestamp;

  /*
   * Only execute callbacks if enough time has passed (throttling)
   */
  if (notificationTimestampDiff.count() >= static_cast<long long>(throttleMs)) {
    executeCallbacks();
  } else {
    pendingNotification = true;
  }
}

void Observer::executeCallbacks() {
  pendingNotification = false;

  auto nextNotificationTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
  );

  /*
   * Calculate scroll velocity
   */
  double nextContainerOffset = container.getContainerOffset();

  if (prevNotificationTimestamp.count() > 0) {
    auto notificationTimestampDiff = nextNotificationTimestamp - prevNotificationTimestamp;
    double notificationTimestampDiffSeconds = notificationTimestampDiff.count() / 1000.0;
    double containerOffsetDiff = nextContainerOffset - prevContainerOffset;

    if (notificationTimestampDiffSeconds > 0.0) {
      scrollVelocity = containerOffsetDiff / notificationTimestampDiffSeconds;
    } else {
      scrollVelocity = 0.0;
    }
  } else {
    scrollVelocity = 0.0;
  }

  /*
   * Update prev values BEFORE executing callbacks
   */
  prevNotificationTimestamp = nextNotificationTimestamp;
  prevContainerOffset = nextContainerOffset;

  auto visibleIndices = container.getVisibleIndices();
  prevMeasurementElementStartIndex = visibleIndices.first;
  prevMeasurementElementEndIndex = visibleIndices.second;

  prevMeasurementContainerHeight = container.nextRevision.measurementContainerHeight;
  prevMeasurementContainerWidth = container.nextRevision.measurementContainerWidth;

  /*
   * Execute all callbacks with the latest Container state
   */
  for (const auto& callback : callbacks) {
    callback.second(container);
  }
}

void Observer::flush() {
  if (pendingNotification) {
    executeCallbacks();
  }
}

bool Observer::hasIndicesChanged() const {
  auto visibleIndices = container.getVisibleIndices();
  std::size_t nextMeasurementElementStartIndex = visibleIndices.first;
  std::size_t nextMeasurementElementEndIndex = visibleIndices.second;

  double nextMeasurementContainerHeight = container.nextRevision.measurementContainerHeight;
  double nextMeasurementContainerWidth = container.nextRevision.measurementContainerWidth;

  /*
   * Check if indices or dimensions have changed
   */
  bool indicesChanged = (nextMeasurementElementStartIndex != prevMeasurementElementStartIndex) ||
    (nextMeasurementElementEndIndex != prevMeasurementElementEndIndex);

  bool dimensionsChanged = (nextMeasurementContainerHeight != prevMeasurementContainerHeight) ||
    (nextMeasurementContainerWidth != prevMeasurementContainerWidth);

  return indicesChanged || dimensionsChanged;
}

double Observer::getScrollVelocity() const {
  return scrollVelocity;
}

}
