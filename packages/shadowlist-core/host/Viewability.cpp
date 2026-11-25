#include <shadowlist-core/host/Viewability.hpp>
#include <shadowlist-core/Container.hpp>
#include <algorithm>
#include <cmath>

#include <utility>

namespace azimgd::shadowlist {

ViewableRule viewableRuleFor(const ViewabilityConfig& config) {
  bool coverage = config.viewAreaCoveragePercentThreshold.has_value();
  double percent =
    coverage ? *config.viewAreaCoveragePercentThreshold : config.itemVisiblePercentThreshold.value_or(0.0);
  return {std::clamp(percent, 0.0, 100.0) / 100.0, coverage};
}

ViewabilityTiming viewabilityTimingFor(const ViewabilityConfig& config) {
  return {config.minimumViewTime > 0.0 ? config.minimumViewTime : 0.0, config.waitForInteraction};
}

bool ViewabilityConfig::operator==(const ViewabilityConfig& other) const {
  auto rule = viewableRuleFor(*this);
  auto otherRule = viewableRuleFor(other);
  auto timing = viewabilityTimingFor(*this);
  auto otherTiming = viewabilityTimingFor(other);
  bool threshold =
    rule.threshold == otherRule.threshold || (std::isnan(rule.threshold) && std::isnan(otherRule.threshold));
  return threshold && rule.coverage == otherRule.coverage && timing.minimumViewTime == otherTiming.minimumViewTime &&
         timing.waitForInteraction == otherTiming.waitForInteraction;
}

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
