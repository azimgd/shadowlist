#include <shadowlist-core/Observer.hpp>
#include <shadowlist-core/Container.hpp>
#include <algorithm>
#include <cmath>

namespace azimgd::shadowlist {

Observer::Observer(Container& container, std::size_t throttleMs)
  : container(container),
    nextSubscriptionId(0),
    throttleMs(throttleMs),
    prevDispatchTimestamp(std::chrono::milliseconds(0)),
    prevMeasurementElementStartIndex(static_cast<std::size_t>(-1)),
    prevMeasurementElementEndIndex(static_cast<std::size_t>(-1)),
    prevMeasurementElementTotalHeight(0.0),
    prevMeasurementElementTotalWidth(0.0),
    prevContainerOffset(0.0),
    scrollVelocity(0.0),
    pendingDispatch(false) {
}

Observer::~Observer() {
  this->callbacks.clear();
}

std::size_t Observer::subscribe(RevisionCallback callback) {
  std::size_t subscriptionId = this->nextSubscriptionId++;
  this->callbacks.push_back({subscriptionId, callback});
  return subscriptionId;
}

void Observer::unsubscribe(std::size_t subscriptionId) {
  this->callbacks.erase(
    std::remove_if(this->callbacks.begin(), this->callbacks.end(),
      [subscriptionId](const std::pair<std::size_t, RevisionCallback>& pair) {
        return pair.first == subscriptionId;
      }),
    this->callbacks.end()
  );
}

void Observer::notifyEndRevision() {
  /*
   * Nothing to dispatch when nothing changed and no throttled dispatch is pending
   */
  if (!hasIndicesChanged() && !this->pendingDispatch) {
    return;
  }

  auto nextDispatchTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
  );
  auto dispatchTimestampDiff = nextDispatchTimestamp - this->prevDispatchTimestamp;

  /*
   * Only execute callbacks if enough time has passed (throttling)
   * A pending dispatch left over from a throttled change is flushed here once
   * the throttle window elapses, so the trailing update is never lost
   */
  if (dispatchTimestampDiff.count() >= static_cast<long long>(this->throttleMs)) {
    executeCallbacks();
  } else {
    this->pendingDispatch = true;
  }
}

void Observer::executeCallbacks() {
  this->pendingDispatch = false;

  auto nextDispatchTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
  );

  /*
   * Calculate scroll velocity
   */
  double nextContainerOffset = this->container.getContainerOffset();

  if (this->prevDispatchTimestamp.count() > 0) {
    auto dispatchTimestampDiff = nextDispatchTimestamp - this->prevDispatchTimestamp;
    double dispatchTimestampDiffSeconds = dispatchTimestampDiff.count() / 1000.0;
    double containerOffsetDiff = nextContainerOffset - this->prevContainerOffset;

    if (dispatchTimestampDiffSeconds > 0.0) {
      this->scrollVelocity = containerOffsetDiff / dispatchTimestampDiffSeconds;
    } else {
      this->scrollVelocity = 0.0;
    }
  } else {
    this->scrollVelocity = 0.0;
  }

  /*
   * Update prev values BEFORE executing callbacks
   */
  this->prevDispatchTimestamp = nextDispatchTimestamp;
  this->prevContainerOffset = nextContainerOffset;

  auto visibleIndices = this->container.getVisibleIndices();
  this->prevMeasurementElementStartIndex = visibleIndices.first;
  this->prevMeasurementElementEndIndex = visibleIndices.second;

  this->prevMeasurementElementTotalHeight = this->container.nextRevision.measurementElementTotalHeight;
  this->prevMeasurementElementTotalWidth = this->container.nextRevision.measurementElementTotalWidth;

  /*
   * Execute all callbacks with the latest Container state
   */
  for (const auto& callback : this->callbacks) {
    callback.second(this->container);
  }
}

void Observer::flush() {
  if (this->pendingDispatch) {
    executeCallbacks();
  }
}

bool Observer::hasIndicesChanged() const {
  auto visibleIndices = this->container.getVisibleIndices();
  std::size_t nextMeasurementElementStartIndex = visibleIndices.first;
  std::size_t nextMeasurementElementEndIndex = visibleIndices.second;

  double nextMeasurementElementTotalHeight = this->container.nextRevision.measurementElementTotalHeight;
  double nextMeasurementElementTotalWidth = this->container.nextRevision.measurementElementTotalWidth;

  /*
   * Check if indices or dimensions have changed
   */
  bool indicesChanged = (nextMeasurementElementStartIndex != this->prevMeasurementElementStartIndex) ||
    (nextMeasurementElementEndIndex != this->prevMeasurementElementEndIndex);

  bool dimensionsChanged = (nextMeasurementElementTotalHeight != this->prevMeasurementElementTotalHeight) ||
    (nextMeasurementElementTotalWidth != this->prevMeasurementElementTotalWidth);

  return indicesChanged || dimensionsChanged;
}

double Observer::getScrollVelocity() const {
  return this->scrollVelocity;
}

}
