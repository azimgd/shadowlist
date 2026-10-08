#pragma once

#include <cstddef>
#include <vector>

namespace azimgd::shadowlist {

/*
 * How far past the action buttons a row follows the finger, as a share of the extra drag.
 */
constexpr double SWIPE_RUBBER_BAND = 0.25;

/*
 * Share of the row's cross size a full swipe has to pass before letting go performs the first
 * action.
 */
constexpr double SWIPE_FULL_FRACTION = 0.55;

/*
 * Narrowest action button and the room around its title, in points, or in dp that the Android
 * kit scales to pixels.
 */
constexpr double SWIPE_BUTTON_MIN = 74.0;
constexpr double SWIPE_BUTTON_PADDING = 24.0;

/*
 * Speed toward a side past which a released row counts as flung, in points or dp per second.
 */
constexpr double SWIPE_FLING_VELOCITY = 300.0;

/*
 * How long a released row takes to come to rest, in milliseconds.
 */
constexpr double SWIPE_SETTLE_DURATION_MS = 250.0;

/*
 * Which side of a row shows its actions. Leading is the cross axis start, left in a vertical
 * list.
 */
enum class SwipeSide {
  None,
  Leading,
  Trailing,
};

/*
 * The swipe actions a row offers. Widths are the action buttons' total cross size, 0 for a
 * side without actions. A full swipe performs the side's first action. rowSize is the row's
 * cross size.
 */
struct SwipeSpec {
  double leadingWidth = 0.0;
  double trailingWidth = 0.0;
  bool leadingFullSwipe = false;
  bool trailingFullSwipe = false;
  double rowSize = 0.0;
};

/*
 * Where a released swipe goes. Full means the row slides out and the side's first action runs.
 */
struct SwipeRest {
  SwipeSide side = SwipeSide::None;
  bool full = false;
  double offset = 0.0;
};

/*
 * A span across the row, from the row's cross axis start.
 */
struct SwipeSpan {
  double start = 0.0;
  double size = 0.0;
};

/*
 * Cross size of an action button whose title and image fit in fitted: the title with room
 * around it, never narrower than SWIPE_BUTTON_MIN. scale turns the constants into host units,
 * 1 for points and the density for Android pixels.
 */
double swipeButtonSize(double fitted, double scale);

/*
 * Where the buttons of the side a row moved offset across the axis reveals go. sizes are that
 * side's button sizes, the first action at the outer edge. The buttons stretch over the gap the
 * row leaves in proportion to their sizes. With full the first button fills the gap and the
 * others get size 0. Each start is the sum of the sizes before it, which lets a host round
 * every edge to its pixels without the buttons drifting apart. crossSize is the row's size.
 */
void swipeButtonSpans(
  const std::vector<double>& sizes,
  double offset,
  bool full,
  double crossSize,
  std::vector<SwipeSpan>& out);

/*
 * The part of the row's cross axis the buttons show through: the gap the row moved offset
 * leaves at the side it reveals.
 */
SwipeSpan swipeRevealedSpan(double offset, double crossSize);

/*
 * The cross axis offset of a row swiped to show its actions, shared by both native lists. A
 * positive offset moves the row toward the trailing side and shows the leading actions. The row
 * follows the finger up to its actions, then slower, or all the way across when a full swipe is
 * allowed. Letting go opens the side past half its actions or flung toward it, performs a full
 * swipe past SWIPE_FULL_FRACTION of the row, and closes otherwise.
 */
class SwipeReveal final {
public:
  void begin(const SwipeSpec& spec, double startOffset);
  const SwipeSpec& getSpec() const { return spec_; }

  /*
   * The offset for a finger that moved translation since begin.
   */
  double drag(double translation) const;

  /*
   * Whether letting go at offset performs a full swipe.
   */
  bool isPastFullSwipe(double offset) const;

  /*
   * Where the row goes when let go at offset with velocity along the cross axis, in host
   * units per second. Flings faster than flingVelocity count as flung.
   */
  SwipeRest settle(double offset, double velocity, double flingVelocity) const;

  /*
   * The offset that keeps a side open.
   */
  double openOffset(SwipeSide side) const;

  /*
   * Whether a row at offset slid all the way out, like after a full swipe.
   */
  bool isSwipedOut(double offset) const;

private:
  SwipeSpec spec_;
  double startOffset_ = 0.0;
};

}
