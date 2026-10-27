#pragma once

#include <shadowlist-core/Constants.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Drag event types a host writes into the list state when a row is picked up or dropped.
 * DRAG_EVENT_LAND marks a row from another list of a drag board landing in this one. It sends
 * JS nothing and only tells the core to keep the offset, see DropHint. Android's
 * ShadowListDragController.DRAG_EVENT_* constants must match.
 */
constexpr int DRAG_EVENT_START = 1;
constexpr int DRAG_EVENT_END = 3;
constexpr int DRAG_EVENT_LAND = 4;

/*
 * How a held row looks and moves in every host: its scale while held, and how long the lift,
 * the other rows' slide and the drop take, in milliseconds. The drop is the held row settling
 * into its slot, or flying back on a cancel.
 */
constexpr double DRAG_LIFT_SCALE = 1.03;
constexpr double DRAG_LIFT_DURATION_MS = 200.0;
constexpr double DRAG_SHIFT_DURATION_MS = 220.0;
constexpr double DRAG_DROP_DURATION_MS = 250.0;

/*
 * The press that picks a row up: how long the finger holds, and how far it may drift during
 * the hold, in points or dp. The drift allowed is wider than a tap's because a finger that means to
 * drag starts moving before the lift. Along the scroll axis the press fails sooner, at
 * DRAG_PRESS_SCROLL_SLOP. A slow scroll then stays a scroll instead of lifting a row.
 */
constexpr double DRAG_PRESS_DURATION_MS = 200.0;
constexpr double DRAG_PRESS_SLOP = 20.0;
constexpr double DRAG_PRESS_SCROLL_SLOP = 8.0;

/*
 * Whether a press still picks a row up after the finger moved this far from where it went
 * down, along and across the scroll axis.
 */
bool dragPressHolds(double along, double cross);

/*
 * The lifted row's shadow: black at this opacity, blurred by the radius and moved down by the
 * offset, in points. Android lifts the row by DRAG_LIFT_ELEVATION dp instead and its renderer
 * draws a shadow of the same shape. Every host shades the same shape: the first view with a
 * background down the row's single child chain, or the row's bounds.
 */
constexpr double DRAG_LIFT_SHADOW_OPACITY = 0.25;
constexpr double DRAG_LIFT_SHADOW_RADIUS = 8.0;
constexpr double DRAG_LIFT_SHADOW_OFFSET = 4.0;
constexpr double DRAG_LIFT_ELEVATION = 8.0;

/*
 * How long a drop within one list waits for its reorder to land before it settles anyway, in
 * milliseconds. See DropSettle.
 */
constexpr double DRAG_DROP_SETTLE_TIMEOUT_MS = 300.0;

/*
 * The shortest time between two haptic ticks, in milliseconds. A drop spot that flickers
 * between two rows must not buzz on every frame. See DragFeedback.
 */
constexpr double DRAG_FEEDBACK_TICK_INTERVAL_MS = 60.0;

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
 * scrolls by up to maxSpeed per second, faster the closer the finger gets.
 */
struct DragAutoScrollConfig {
  double edge = 0.0;
  double maxSpeed = 0.0;
};

/*
 * iOS and macOS, in points and points per second.
 */
constexpr DragAutoScrollConfig DRAG_AUTO_SCROLL_IOS{90.0, 960.0};

/*
 * Android, in dp and dp per second. The host scales both to pixels.
 */
constexpr DragAutoScrollConfig DRAG_AUTO_SCROLL_ANDROID{60.0, 720.0};

/*
 * Scrolling the view around a drag board sideways while the finger is near its left or right
 * edge, in iOS points. The speed starts at 0 where the edge begins and is full only at the
 * screen's edge, which a thumb seldom reaches. The edge is wide enough for a thumb to sit well
 * inside it. It reaches into the side of a list that fills most of the width. The board
 * only scrolls once the finger has moved toward that edge, see DragBoardScroll.
 */
constexpr DragAutoScrollConfig DRAG_BOARD_SCROLL_IOS{64.0, 960.0};

/*
 * The same for Android, in dp. It relates to the iOS one the way DRAG_AUTO_SCROLL_ANDROID
 * relates to DRAG_AUTO_SCROLL_IOS. The host scales both to pixels. Reserved for Android's
 * drag board, which does not run yet.
 */
constexpr DragAutoScrollConfig DRAG_BOARD_SCROLL_ANDROID{43.0, 720.0};

/*
 * How far the finger moves sideways toward an edge from where it lifted the row before the
 * board scrolls, in iOS points and in dp for Android.
 */
constexpr double DRAG_BOARD_SCROLL_ARM_IOS = 16.0;
constexpr double DRAG_BOARD_SCROLL_ARM_ANDROID = 11.0;

/*
 * The longest frame an auto scroll moves for, in seconds. A frame after a stall scrolls as
 * far as this one would, not as far as the whole stall.
 */
constexpr double DRAG_AUTO_SCROLL_MAX_FRAME_SECONDS = 1.0 / 30.0;

/*
 * Scroll distance for a frame of dtSeconds, negative toward the start. Touch is the finger's
 * position in the viewport. Speeds are per second. Every frame rate scrolls as fast.
 */
double dragAutoScrollDelta(const DragAutoScrollConfig& config, double touch, double windowSize, double dtSeconds);

/*
 * The offset after this frame's auto scroll, kept inside minOffset and maxOffset. minOffset
 * is below 0 for a scroll view with an inset at its start.
 */
double dragAutoScrollOffset(
  const DragAutoScrollConfig& config,
  double touch,
  double windowSize,
  double offset,
  double maxOffset,
  double dtSeconds,
  double minOffset = 0.0);

/*
 * Whether the board scrolls toward the edge the finger is near. direction is that scroll's
 * sign, negative toward the start. The finger must have moved at least armDistance toward the
 * edge from pickup, and must not be heading back away from it. A finger resting at the edge
 * keeps it armed. All values are along the board's scroll axis in one space.
 */
bool dragBoardShouldArm(double pickup, double finger, double velocity, double direction, double armDistance);

/*
 * The sideways scroll of the view around a drag board, from pickup to drop. It keeps the
 * finger's velocity across frames for dragBoardShouldArm.
 */
class DragBoardScroll final {
public:
  void begin(double pickup);

  /*
   * This frame's scroll distance for the view around the board. finger is in the space
   * pickup was given in, touch is the finger in that view's viewport and windowSize is the
   * viewport's size. 0 until the finger arms it.
   */
  double delta(
    const DragAutoScrollConfig& config,
    double armDistance,
    double finger,
    double touch,
    double windowSize,
    double dtSeconds);

  double getVelocity() const { return velocity_; }

private:
  double pickup_ = 0.0;
  double previous_ = 0.0;
  double velocity_ = 0.0;
};

/*
 * A haptic tick each time the drop spot moves, at most once every
 * DRAG_FEEDBACK_TICK_INTERVAL_MS. The host plays the tick when shouldTick says so.
 */
class DragFeedback final {
public:
  /*
   * Start counting from this drop spot, which does not tick. UNDEFINED_INDEX makes the next
   * spot tick, like the first one in a list the row just entered.
   */
  void reset(std::size_t insertionIndex = UNDEFINED_INDEX);

  bool shouldTick(std::size_t insertionIndex, double nowMs);

private:
  std::size_t insertionIndex_ = UNDEFINED_INDEX;
  double lastTickMs_ = -DRAG_FEEDBACK_TICK_INTERVAL_MS;
};

/*
 * After a drop within one list, the rows stay shifted until the reorder lands, which the host
 * checks every frame. A reorder of rows the same size may never publish new state. It
 * watches the dropped row's index reach the drop spot instead.
 */
class DropSettle final {
public:
  /*
   * Waiting goes on. Settle is the row at its drop spot: the host animates it in. GiveUp is no
   * reorder within timeoutMs, or the row unmounted: the host puts every row back at once.
   * Idle is no drop waiting.
   */
  enum class Status { Idle, Waiting, Settle, GiveUp };

  void begin(std::size_t insertionIndex, double nowMs, double timeoutMs = DRAG_DROP_SETTLE_TIMEOUT_MS);

  /*
   * One frame, with the dropped row's index now, or UNDEFINED_INDEX once it unmounted. Settle
   * and GiveUp end the wait.
   */
  Status tick(std::size_t currentIndex, double nowMs);

  void reset() { waiting_ = false; }
  bool isWaiting() const { return waiting_; }
  std::size_t getInsertionIndex() const { return insertionIndex_; }

private:
  bool waiting_ = false;
  std::size_t insertionIndex_ = UNDEFINED_INDEX;
  double deadlineMs_ = 0.0;
};

/*
 * Where the held row's leading edge goes, following the finger and kept inside the content.
 */
double dragHeldLeading(double touchContent, double grabOffset, double extent, double contentSize);

/*
 * The position in rows of the row the held one would drop at, or UNDEFINED_INDEX to stay where
 * it started. It is the farthest row, counted from originIndex, whose midpoint the held row's
 * center has passed. Rows with an UNDEFINED_INDEX index are skipped.
 */
std::size_t dragInsertionIndex(
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
std::size_t dragGridInsertionIndex(
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
 * Drag board: a row dragged out of one list and dropped into another. While the held row
 * hovers another list, its home list closes the gap as if the row were removed and the other
 * list opens a gap for a row it does not own. DragReorder runs these for iOS. The array forms
 * below are reserved for Android's drag board, which does not run yet: dragRemovalShift,
 * dragGridRemovalShifts, dragForeignInsertionIndex, dragGridForeignInsertionIndex and
 * dragGridForeignShifts. See DragBoard.hpp for the board itself.
 */

/*
 * How far a row slides while the held row is away: every row after the origin moves toward it
 * by the held row's extent.
 */
double dragRemovalShift(std::size_t originIndex, double draggedExtent, std::size_t index);

/*
 * How far each grid cell slides while the held cell is away, written into shifts and
 * crossShifts. Cells after the held one are laid out again as if it were removed. A cell can
 * move to another column.
 */
void dragGridRemovalShifts(
  const DragCells& cells,
  const DragRow& held,
  std::size_t columns,
  double* shifts,
  double* crossShifts);

/*
 * The position in rows of the row an incoming row would drop before, or count to drop after
 * every row. It is the row with the lowest index whose midpoint the incoming row's center has
 * not passed. Exactly on a midpoint is not past it. Rows with an UNDEFINED_INDEX index are
 * skipped.
 */
std::size_t dragForeignInsertionIndex(
  const std::size_t* indices,
  const double* leadings,
  const double* extents,
  std::size_t count,
  double center);

/*
 * How far a row slides to open the gap for an incoming row: rows at or after insertionIndex
 * move by the incoming extent away from the start.
 */
double dragForeignShift(std::size_t insertionIndex, double extent, std::size_t index);

/*
 * The position in cells of the cell an incoming cell would drop before in a grid. Over a
 * cell's resting frame it is that cell. It is cells.count to drop after every cell, which
 * happens with no mounted cells, past the trailing edge of every cell, beside the last cell
 * in its row, or when insertionIndex is UNDEFINED_INDEX. Over no cell otherwise it keeps
 * insertionIndex and returns that cell's position, or UNDEFINED_INDEX when it is not mounted.
 */
std::size_t dragGridForeignInsertionIndex(
  const DragCells& cells,
  std::size_t insertionIndex,
  double center,
  double crossCenter);

/*
 * How far each grid cell slides to open the gap for an incoming cell of extent and
 * crossExtent, written into shifts and crossShifts. Cells from insertionIndex onward are laid
 * out again as if the incoming cell sat at insertionIndex. A cell can move to another column.
 */
void dragGridForeignShifts(
  const DragCells& cells,
  double extent,
  double crossExtent,
  std::size_t insertionIndex,
  std::size_t columns,
  double* shifts,
  double* crossShifts);

/*
 * One drag, from pickup to drop. On a drag board the same class runs either side. The home
 * list keeps its normal drag and marks the held row away while it hovers another list. The
 * list under the row runs a foreign session from beginForeign to endForeign. A foreign session
 * and a normal drag never run together on one instance. Calling begin or beginCell ends a
 * foreign session, and beginForeign drops any normal drag.
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
    double contentSize,
    double crossExtent);

  /*
   * A data change during the drag can move the held row. Take its current index and key.
   * An UNDEFINED_INDEX index or empty key keeps the one we have.
   */
  void updateOrigin(std::size_t index, const std::string& key);

  /*
   * Move the held row under the finger and return its translation from its resting place.
   */
  double place(double touchContent, double restingLeading, double extent, double contentSize);

  /*
   * Move the held grid cell under the finger, kept inside the content on both axes, and
   * return its translation from its resting frame.
   */
  DragOffset placeCell(
    double touchContent,
    double touchCross,
    const DragRow& resting,
    double contentSize,
    double crossContentSize);

  /*
   * Find the drop spot for the held row's current place among the other mounted rows.
   * In a grid this also works out every cell's shift for offsetFor. While the held row is
   * away there is no drop spot and a grid works out the shifts that close its gap. In a
   * foreign session it finds the drop spot for the incoming row among this list's mounted
   * rows. getInsertionIndex describes what it reports.
   */
  void updateInsertion(const std::vector<DragRow>& rows);

  /*
   * How far a row slides along the scroll axis. The same as offsetFor without the cross value.
   */
  double shiftFor(std::size_t index) const { return offsetFor(index).leading; }

  /*
   * Mark the held row as hovering another list, or back over its home list. While away the
   * other rows close the gap as if the held row were removed, and getInsertionIndex is
   * UNDEFINED_INDEX with an empty getInsertionKey. A grid works out its shifts in the next
   * updateInsertion. Coming back puts the drop spot on the origin until the next
   * updateInsertion. It does nothing in a foreign session.
   */
  void setHeldAway(bool away);
  bool isHeldAway() const { return heldAway_; }

  /*
   * Start a foreign session for a row from another list, with its extent along and across the
   * scroll axis and this list's column count. There is no origin row. getOriginIndex stays
   * UNDEFINED_INDEX and getInsertionIndex is UNDEFINED_INDEX until the first updateInsertion.
   */
  void beginForeign(double extent, double crossExtent, std::size_t columns);

  /*
   * Place the incoming row's center in this list's content, along and across the scroll axis.
   */
  void placeForeignCenter(double center, double crossCenter);

  /*
   * End the foreign session. Nothing is held and no row shifts.
   */
  void endForeign();
  bool isForeign() const { return foreign_; }

  /*
   * How far a row or cell slides to open the gap, on both axes.
   */
  DragOffset offsetFor(std::size_t index) const;

  bool isGrid() const { return columns_ > 1; }

  std::size_t getOriginIndex() const { return originIndex_; }

  /*
   * In a normal drag, the index of the row the held one drops at, and the origin when it
   * stays. In a foreign session, the index the incoming row takes in this list: the row at
   * that index and every one after it move one place toward the end. With the center past
   * every mounted row it is one past the last mounted row's index, or 0 with none mounted,
   * and getInsertionKey is empty. Otherwise getInsertionKey is the key of the row the incoming
   * one drops before. dragBoardDropIndex turns it into the index a drop sends.
   */
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
  void updateForeignInsertion(const std::vector<DragRow>& rows);

  /*
   * Fill offsets_ for the mounted rows. relayout(write) calls write(position, offset).
   */
  template <typename Relayout>
  void storeOffsets(const std::vector<DragRow>& rows, Relayout relayout);

  bool heldAway_ = false;
  bool foreign_ = false;

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

  /*
   * Grid shifts from the previous updateInsertion, by index from offsetsBase_.
   */
  std::size_t offsetsBase_ = 0;
  std::vector<DragOffset> offsets_;
};

}
