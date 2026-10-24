#pragma once

#include <shadowlist-core/host/MountedRange.hpp>

#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * The timing half of a viewability config. The threshold half is a ViewableRule the core
 * applies when it picks the viewable rows.
 */
struct ViewabilityTiming {
  /*
   * How long, in milliseconds, rows must stay viewable before they are reported. Each change
   * starts the wait again, and the rows viewable when it ends are reported.
   */
  double minimumViewTime = 0.0;

  /*
   * Nothing is viewable before recordInteraction.
   */
  bool waitForInteraction = false;
};

/*
 * One report: every viewable key in order, and what changed since the previous report.
 */
struct ViewabilityReport {
  std::vector<std::string> viewable;
  ViewableChanges changes;
};

/*
 * What one viewability config reported last, compared by key so a reorder inside the viewable
 * rows reports nothing. Only a change reports. Times are in milliseconds from any fixed start.
 */
class ViewabilityTracker final {
public:
  void setTiming(const ViewabilityTiming& timing);

  /*
   * The viewable rows, after the range or the rows in it changed. The same keys again change
   * nothing.
   */
  void update(std::vector<std::string> viewable, double now);

  void recordInteraction(double now);

  /*
   * Whether a report waits for minimumViewTime to pass.
   */
  bool isWaiting() const {
    return deadline_ >= 0.0;
  }

  /*
   * The report due at now, if any. Returns false when nothing is due or nothing changed.
   */
  bool take(double now, ViewabilityReport& report);

private:
  ViewabilityTiming timing_;
  std::vector<std::string> current_;
  std::vector<std::string> reported_;
  bool interacted_ = false;

  /*
   * When the pending report is due, or negative for none.
   */
  double deadline_ = -1.0;
};

}
