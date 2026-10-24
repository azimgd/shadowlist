#include <shadowlist-core/host/Viewability.hpp>

#include <utility>

namespace azimgd::shadowlist {

void ViewabilityTracker::setTiming(const ViewabilityTiming& timing) {
  timing_ = timing;
}

void ViewabilityTracker::update(std::vector<std::string> viewable, double now) {
  // The same rows, like a data change elsewhere in the list, are no change and keep the wait.
  if (viewable == current_ && (deadline_ >= 0.0 || current_ == reported_)) {
    return;
  }
  current_ = std::move(viewable);
  if (timing_.waitForInteraction && !interacted_) {
    return;
  }
  deadline_ = now + (timing_.minimumViewTime > 0.0 ? timing_.minimumViewTime : 0.0);
}

void ViewabilityTracker::recordInteraction(double now) {
  if (interacted_) {
    return;
  }
  interacted_ = true;
  deadline_ = now + (timing_.minimumViewTime > 0.0 ? timing_.minimumViewTime : 0.0);
}

bool ViewabilityTracker::take(double now, ViewabilityReport& report) {
  if (deadline_ < 0.0 || now < deadline_) {
    return false;
  }
  deadline_ = -1.0;
  ViewableChanges changes = viewableChanges(reported_, current_);
  reported_ = current_;
  if (changes.isEmpty()) {
    return false;
  }
  report.viewable = reported_;
  report.changes = std::move(changes);
  return true;
}

}
