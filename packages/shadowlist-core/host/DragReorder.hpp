#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Drag to reorder along the scroll axis. The host owns the gesture, the views and the
 * scrolling. This works out where the held row goes, where it would drop and how far every
 * other row slides to open the gap.
 */

/*
 * A mounted row as the drag sees it, without any drag translation. Leading and extent are
 * along the scroll axis, in the content.
 */
struct DragRow {
  long index = -1;
  std::string key;
  double leading = 0.0;
  double extent = 0.0;
};

/*
 * Scrolling near the viewport edges while a row is held. Within edge of either end the list
 * scrolls by up to maxSpeed per frame, faster the closer the finger gets.
 */
struct DragAutoScrollConfig {
  double edge = 0.0;
  double maxSpeed = 0.0;
};

// iOS, in points.
constexpr DragAutoScrollConfig DRAG_AUTO_SCROLL_IOS{90.0, 16.0};

// Android, in dp. The host scales both to pixels.
constexpr DragAutoScrollConfig DRAG_AUTO_SCROLL_ANDROID{60.0, 12.0};

/*
 * Scroll distance for this frame, negative toward the start. Touch is the finger's position
 * in the viewport.
 */
double dragAutoScrollDelta(const DragAutoScrollConfig& config, double touch, double windowSize);

/*
 * The offset after this frame's auto scroll, kept inside 0 and maxOffset.
 */
double dragAutoScrollOffset(const DragAutoScrollConfig& config, double touch, double windowSize, double offset, double maxOffset);

/*
 * Where the held row's leading edge goes, following the finger and kept inside the content.
 */
double dragHeldLeading(double touchContent, double grabOffset, double extent, double contentExtent);

/*
 * The position in rows of the row the held one would drop at, or -1 to stay where it
 * started. It is the farthest row, counted from originIndex, whose midpoint the held row's
 * center has passed. Rows with a negative index are skipped.
 */
long dragInsertionPosition(const long* indices, const double* leadings, const double* extents, std::size_t count, long originIndex, double center);

/*
 * How far a row slides to open the gap: rows between the origin and the drop spot move by
 * the held row's extent toward the origin.
 */
double dragShift(long originIndex, long insertionIndex, double draggedExtent, long index);

/*
 * One drag, from pickup to drop.
 */
class DragReorder {
public:
  /*
   * Start with the picked up row, its resting leading edge and extent, and where the finger
   * touched in the content.
   */
  void begin(long index, std::string key, double restingLeading, double extent, double touchContent);

  /*
   * A data change during the drag can move the held row. Take its current index and key.
   * A negative index or empty key keeps the one we have.
   */
  void updateOrigin(long index, const std::string& key);

  /*
   * Move the held row under the finger and return its translation from its resting place.
   */
  double place(double touchContent, double restingLeading, double extent, double contentExtent);

  /*
   * Find the drop spot for the held row's current place among the other mounted rows.
   */
  void updateInsertion(const std::vector<DragRow>& rows);

  double shiftFor(long index) const {
    return dragShift(originIndex_, insertionIndex_, draggedExtent_, index);
  }

  long originIndex() const { return originIndex_; }
  long insertionIndex() const { return insertionIndex_; }
  const std::string& originKey() const { return originKey_; }
  const std::string& insertionKey() const { return insertionKey_; }
  // The held row's leading edge on the last placed frame, where it was let go on drop.
  double leading() const { return leading_; }

private:
  long originIndex_ = -1;
  long insertionIndex_ = -1;
  std::string originKey_;
  std::string insertionKey_;
  double draggedExtent_ = 0.0;
  double grabOffset_ = 0.0;
  double leading_ = 0.0;
  double center_ = 0.0;
};

}
