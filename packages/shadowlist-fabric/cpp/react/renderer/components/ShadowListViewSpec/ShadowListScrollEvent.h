#pragma once

#include <jsi/jsi.h>

#include <chrono>

namespace facebook::react {

/*
 * What one scroll event reports, in points. Velocity is in points per millisecond along each
 * axis, positive toward the end of the content.
 */
struct ShadowListScrollMetrics {
  double offsetX = 0.0;
  double offsetY = 0.0;
  double contentWidth = 0.0;
  double contentHeight = 0.0;
  double viewportWidth = 0.0;
  double viewportHeight = 0.0;
  double velocityX = 0.0;
  double velocityY = 0.0;
};

/*
 * The payload React Native's ScrollView sends for onScroll and the drag and momentum events,
 * plus the flat contentOffsetX and contentOffsetY. The list has no insets and no zoom.
 */
inline jsi::Object shadowListScrollPayload(jsi::Runtime& runtime, const ShadowListScrollMetrics& metrics) {
  auto point = [&runtime](double x, double y) {
    auto value = jsi::Object(runtime);
    value.setProperty(runtime, "x", x);
    value.setProperty(runtime, "y", y);
    return value;
  };
  auto size = [&runtime](double width, double height) {
    auto value = jsi::Object(runtime);
    value.setProperty(runtime, "width", width);
    value.setProperty(runtime, "height", height);
    return value;
  };
  auto inset = jsi::Object(runtime);
  inset.setProperty(runtime, "top", 0.0);
  inset.setProperty(runtime, "left", 0.0);
  inset.setProperty(runtime, "bottom", 0.0);
  inset.setProperty(runtime, "right", 0.0);

  auto payload = jsi::Object(runtime);
  payload.setProperty(runtime, "contentOffsetX", metrics.offsetX);
  payload.setProperty(runtime, "contentOffsetY", metrics.offsetY);
  payload.setProperty(runtime, "contentOffset", point(metrics.offsetX, metrics.offsetY));
  payload.setProperty(runtime, "contentSize", size(metrics.contentWidth, metrics.contentHeight));
  payload.setProperty(runtime, "layoutMeasurement", size(metrics.viewportWidth, metrics.viewportHeight));
  payload.setProperty(runtime, "velocity", point(metrics.velocityX, metrics.velocityY));
  payload.setProperty(runtime, "contentInset", inset);
  payload.setProperty(runtime, "zoomScale", 1.0);
  return payload;
}

/*
 * Velocity from the offsets the core sees, and the scrollEventThrottle gate. One per list,
 * shared by its scroll callback.
 */
class ShadowListScrollTracker final {
public:
  explicit ShadowListScrollTracker(double throttleMs) : throttleMs_(throttleMs) {}

  /*
   * Record an offset and fill the velocity in metrics. Returns whether to send the event now.
   */
  bool track(ShadowListScrollMetrics& metrics) {
    double now = nowMs();
    if (hasPrevious_ && now > previousTimeMs_) {
      double elapsed = now - previousTimeMs_;
      metrics.velocityX = (metrics.offsetX - previousX_) / elapsed;
      metrics.velocityY = (metrics.offsetY - previousY_) / elapsed;
    }
    hasPrevious_ = true;
    previousX_ = metrics.offsetX;
    previousY_ = metrics.offsetY;
    previousTimeMs_ = now;
    if (throttleMs_ > 0.0 && hasSent_ && now - sentTimeMs_ < throttleMs_) {
      pending_ = true;
      pendingMetrics_ = metrics;
      return false;
    }
    hasSent_ = true;
    sentTimeMs_ = now;
    pending_ = false;
    return true;
  }

  /*
   * The newest event the throttle dropped, once the scroll came to rest. Without it the last
   * event JS saw would hold an offset from before the scroll stopped.
   */
  bool takeTrailing(ShadowListScrollMetrics& metrics) {
    if (!pending_) {
      return false;
    }
    pending_ = false;
    hasSent_ = true;
    sentTimeMs_ = nowMs();
    metrics = pendingMetrics_;
    metrics.velocityX = 0.0;
    metrics.velocityY = 0.0;
    return true;
  }

private:
  static double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
  }

  double throttleMs_ = 0.0;
  bool hasPrevious_ = false;
  double previousX_ = 0.0;
  double previousY_ = 0.0;
  double previousTimeMs_ = 0.0;
  bool hasSent_ = false;
  double sentTimeMs_ = 0.0;
  bool pending_ = false;
  ShadowListScrollMetrics pendingMetrics_;
};

}
