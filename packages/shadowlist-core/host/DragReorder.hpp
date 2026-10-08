#pragma once

#include <shadowlist-core/Constants.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Drag event types a host writes into the list state when a row is picked up or dropped.
 * Android's ShadowListDragController.DRAG_EVENT_* constants must match.
 */
constexpr int DRAG_EVENT_START = 1;
constexpr int DRAG_EVENT_END = 3;

/*
 * How a held row looks and moves in both kits: its scale while held, and how long the lift,
 * the other rows' slide and the drop take, in milliseconds.
 */
constexpr double DRAG_LIFT_SCALE = 1.03;
constexpr double DRAG_LIFT_DURATION_MS = 200.0;
constexpr double DRAG_SHIFT_DURATION_MS = 220.0;
constexpr double DRAG_DROP_DURATION_MS = 250.0;

/*
 * Drag to reorder along the scroll axis, or across the cells of a grid. The host owns the
 * gesture, the views and the scrolling. This works out where the held row goes, where it
 * would drop and how far every other row slides to open the gap.
 */

/*
 * A mounted row as the drag sees it, without any drag translation. Leading and extent are
 * along the scroll axis, in the content. The cross values place a grid cell in its column
 * and stay 0 for a single column.
 */
struct DragRow {
  std::size_t index = UNDEFINED_INDEX;
  std::string key;
  double leading = 0.0;
  double extent = 0.0;
  double crossLeading = 0.0;
  double crossExtent = 0.0;
};

/*
 * A translation along the scroll axis and across it.
 */
struct DragOffset {
  double leading = 0.0;
  double cross = 0.0;
};

/*
 * Mounted grid cells as parallel arrays, for hosts that reuse buffers across frames.
 */
struct DragCells {
  const std::size_t* indices = nullptr;
  const double* leadings = nullptr;
  const double* extents = nullptr;
  const double* crossLeadings = nullptr;
  const double* crossExtents = nullptr;
  std::size_t count = 0;
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
double dragAutoScrollOffset(
  const DragAutoScrollConfig& config,
  double touch,
  double windowSize,
  double offset,
  double maxOffset);

/*
 * Where the held row's leading edge goes, following the finger and kept inside the content.
 */
double dragHeldLeading(double touchContent, double grabOffset, double extent, double contentExtent);

/*
 * The position in rows of the row the held one would drop at, or UNDEFINED_INDEX to stay where
 * it started. It is the farthest row, counted from originIndex, whose midpoint the held row's
 * center has passed. Rows with an UNDEFINED_INDEX index are skipped.
 */
std::size_t dragInsertionPosition(
  const std::size_t* indices,
  const double* leadings,
  const double* extents,
  std::size_t count,
  std::size_t originIndex,
  double center);

/*
 * How far a row slides to open the gap: rows between the origin and the drop spot move by
 * the held row's extent toward the origin.
 */
double dragShift(std::size_t originIndex, std::size_t insertionIndex, double draggedExtent, std::size_t index);

/*
 * The position in cells of the cell the held one would drop at in a grid, or UNDEFINED_INDEX
 * for its own slot. The drop spot is the cell whose resting frame holds the held cell's center. Over its
 * own resting frame it goes back to the origin, and over no cell it keeps insertionIndex.
 */
std::size_t dragGridInsertionPosition(
  const DragCells& cells,
  const DragRow& held,
  std::size_t insertionIndex,
  double center,
  double crossCenter);

/*
 * How far each grid cell slides to open the gap, written into shifts and crossShifts. Cells
 * are laid out again the way the core does. A cell can move to another column.
 */
void dragGridShifts(
  const DragCells& cells,
  const DragRow& held,
  std::size_t insertionIndex,
  std::size_t columns,
  double* shifts,
  double* crossShifts);

/*
 * One drag, from pickup to drop.
 */
class DragReorder final {
public:
  /*
   * Start with the picked up row, its resting leading edge and extent, and where the finger
   * touched in the content.
   */
  void begin(std::size_t index, std::string key, double restingLeading, double extent, double touchContent);

  /*
   * Start a drag in a grid of columns, with the picked up cell's resting frame and where
   * the finger touched in the content, along and across the scroll axis.
   */
  void beginCell(const DragRow& resting, double touchContent, double touchCross, std::size_t columns);

  /*
   * Start a drag in a list or, with columns above 1, in a grid. The same as begin or
   * beginCell, picked by the column count.
   */
  void begin(const DragRow& resting, double touchAlong, double touchCross, std::size_t columns);

  /*
   * Move the held row or grid cell under the finger and return its translation from its
   * resting place. The same as place or placeCell, picked by the kind of drag.
   */
  DragOffset placeRow(
    double touchAlong,
    double touchCross,
    const DragRow& resting,
    double contentExtent,
    double crossExtent);

  /*
   * A data change during the drag can move the held row. Take its current index and key.
   * An UNDEFINED_INDEX index or empty key keeps the one we have.
   */
  void updateOrigin(std::size_t index, const std::string& key);

  /*
   * Move the held row under the finger and return its translation from its resting place.
   */
  double place(double touchContent, double restingLeading, double extent, double contentExtent);

  /*
   * Move the held grid cell under the finger, kept inside the content on both axes, and
   * return its translation from its resting frame.
   */
  DragOffset placeCell(
    double touchContent,
    double touchCross,
    const DragRow& resting,
    double contentExtent,
    double crossContentExtent);

  /*
   * Find the drop spot for the held row's current place among the other mounted rows.
   * In a grid this also works out every cell's shift for offsetFor.
   */
  void updateInsertion(const std::vector<DragRow>& rows);

  double shiftFor(std::size_t index) const {
    return dragShift(originIndex_, insertionIndex_, draggedExtent_, index);
  }

  /*
   * How far a row or cell slides to open the gap, on both axes.
   */
  DragOffset offsetFor(std::size_t index) const;

  bool isGrid() const { return columns_ > 1; }

  std::size_t getOriginIndex() const { return originIndex_; }
  std::size_t getInsertionIndex() const { return insertionIndex_; }
  const std::string& getOriginKey() const { return originKey_; }
  const std::string& getInsertionKey() const { return insertionKey_; }
  /*
   * The held row's leading edge where it was placed, which is where it was let go on drop.
   */
  double getLeading() const { return leading_; }
  double getCrossLeading() const { return crossLeading_; }

private:
  void updateGridInsertion(const std::vector<DragRow>& rows);

  std::size_t originIndex_ = UNDEFINED_INDEX;
  std::size_t insertionIndex_ = UNDEFINED_INDEX;
  std::string originKey_;
  std::string insertionKey_;
  double draggedExtent_ = 0.0;
  double grabOffset_ = 0.0;
  double leading_ = 0.0;
  double center_ = 0.0;
  std::size_t columns_ = 1;
  double crossGrabOffset_ = 0.0;
  double crossLeading_ = 0.0;
  double crossCenter_ = 0.0;
  DragRow heldResting_;
  // Grid shifts from the previous updateInsertion, by index from offsetsBase_.
  std::size_t offsetsBase_ = 0;
  std::vector<DragOffset> offsets_;
};

}
