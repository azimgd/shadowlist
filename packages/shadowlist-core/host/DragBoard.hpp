#pragma once

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/host/DragReorder.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * A drag board: lists that share a board id, where a row dragged out of one list drops into
 * another. Each list runs its own DragReorder: the home list marks the held row away while it
 * hovers another list, and the list under it runs a foreign session. DragBoard holds what the
 * lists share: which list the row is over, where its floating picture goes, and the wait for
 * the row to land in the list it dropped in. The host keeps the views, the gestures, the
 * animations and the hit test that finds the lists under the finger.
 */

/*
 * How a drop into another list waits for its row to mount there before the host cleans up
 * anyway, in milliseconds. It is longer than a drop within one list because JS changes two
 * lists' data first, and the move may never land when JS does not handle it.
 */
constexpr double DRAG_BOARD_LAND_TIMEOUT_MS = 1000.0;

/*
 * How long the floating picture takes to move from where the row showed in its home list to
 * its place under the finger, in milliseconds. In its home list the row only follows the
 * finger along the scroll axis. Lifted out, it catches up with the finger sideways.
 */
constexpr double DRAG_BOARD_LIFT_OUT_MS = 100.0;

/*
 * A point in the window, in points or pixels.
 */
struct DragPoint {
  double x = 0.0;
  double y = 0.0;
};

/*
 * The index a row dropped into another list takes there, or UNDEFINED_INDEX for no drop.
 * insertionIndex and insertionKey are the target's foreign session values. An empty key means
 * the row goes after every row, which is rowCount. Otherwise the index is kept inside rowCount
 * because rows the core counted may have left the data. A rowCount of UNDEFINED_INDEX means
 * the host does not know it and the index is kept as it is.
 */
std::size_t dragBoardDropIndex(std::size_t insertionIndex, const std::string& insertionKey, std::size_t rowCount);

/*
 * Where the held row is in a board drag. AtHome is over its home list, where the normal drag
 * goes on. Floating is lifted out over the board, with the last list it hovered as the
 * target. Landing is after a drop into the target, until the row shows up in the target's
 * keys or DRAG_BOARD_LAND_TIMEOUT_MS passes. Settling is the picture flying onto the landed
 * row, until the host calls endFlight. Idle is before begin and after the flight ends.
 */
enum class DragBoardPhase { Idle, AtHome, Floating, Landing, Settling };

/*
 * What the host does after a hover. LiftOut lifts the row out of its home list and starts a
 * foreign session in next. ReturnHome ends the foreign session in previous and puts the row
 * back into its home list. Retarget ends the foreign session in previous and starts one in
 * next. Stay changes nothing. An empty previous names no list, after the target left the
 * board.
 */
struct DragBoardStep {
  enum class Kind { Stay, LiftOut, ReturnHome, Retarget };
  Kind kind = Kind::Stay;
  std::string previous;
  std::string next;
};

/*
 * One drag frame on a board. step is the hover's change. While floating the host shows the
 * row's picture centered on overlayCenter, in window coordinates. hideHeld says whether the
 * home list hides its held row, which it does from lift out until the flight ends.
 */
struct DragBoardFrame {
  DragBoardStep step;
  bool floating = false;
  DragPoint overlayCenter;
  bool hideHeld = false;
};

/*
 * How a release ends. Home lets the home list run its normal drop. Cancel puts the row back
 * into its home list. Drop moves it into listId at index, before the row keyed key, or after
 * every row when key is empty.
 */
struct DragBoardRelease {
  enum class Kind { Home, Cancel, Drop };
  Kind kind = Kind::Home;
  std::string listId;
  std::size_t index = UNDEFINED_INDEX;
  std::string key;
};

/*
 * One drag across the lists of a board, from pickup until the row settles. Lists are named by
 * ids that are unique on the board and not empty. The row lifts out of its home list only over
 * another list. Between lists the last one hovered stays the target, and over the home list
 * the row comes back. Each flight gets a new getFlight number. A host timer or animation from
 * an older flight can tell by it that it no longer applies.
 */
class DragBoard final {
public:
  /*
   * A pickup in homeId of the row keyed heldKey. A row without a key can float but never
   * drops into another list. finger and heldCenter are in window coordinates: the finger and
   * the center of the held row where it rests. The floating picture keeps that grab.
   */
  void begin(std::string homeId, std::string heldKey, DragPoint finger = {}, DragPoint heldCenter = {});

  /*
   * Every drag frame, with the ids of the lists that are under the finger and can take the
   * row, topmost first. Several ids mean the lists overlap, and the home list wins among them.
   * Otherwise the first id is the one hovered. Empty ids are skipped.
   */
  DragBoardStep hover(const std::vector<std::string>& candidates);

  /*
   * hover, and where the floating picture goes. finger is in window coordinates, shownCenter
   * is the held row's center where it shows now in its home list, which is where the picture
   * starts on lift out.
   */
  DragBoardFrame frame(
    DragPoint finger,
    DragPoint shownCenter,
    const std::vector<std::string>& candidates,
    double nowMs);

  /*
   * Let go. The target's foreign session gives insertionIndex and insertionKey, or
   * UNDEFINED_INDEX when it has no drop spot. rowCount is the target's row count, as for
   * dragBoardDropIndex. Over the home list it is Home. Floating it drops into the target,
   * which is the last list hovered even when the finger is now between lists. It cancels
   * with no target, no drop spot or no held key. A drop starts Landing, with getTargetId the
   * list it lands in. Every other release ends the flight.
   */
  DragBoardRelease release(
    std::size_t insertionIndex,
    const std::string& insertionKey,
    std::size_t rowCount,
    double nowMs = 0.0);

  /*
   * While Landing, whether the row has landed in listId, whose current keys are keys. The
   * first true moves to Settling, and the host settles the picture onto the row. The target
   * stops opening its gap then, since its keys hold the row in its new place.
   */
  bool landed(const std::string& listId, const std::vector<std::string>& keys);

  /*
   * The same with the check done by the host, for hosts that look the key up themselves.
   */
  bool landed(const std::string& listId, bool keysHoldHeldKey);

  /*
   * While Landing, whether the wait ran out at nowMs. The first true ends the flight.
   */
  bool expired(double nowMs);

  /*
   * The drag stops without a drop. The board goes idle and returns the target, empty for
   * none, whose foreign session the host ends.
   */
  std::string cancel();

  /*
   * The flight is over: the picture settled, or the host gave up. The board goes idle.
   */
  void endFlight() { reset(); }

  /*
   * A list leaves the board mid drag. When it is the home list of a drag still deciding, the
   * board goes idle. When it is the target of a floating row the row floats on without one
   * until it hovers another list, and a release before then cancels. When it is the list a row
   * lands in, the flight ends.
   */
  void leave(const std::string& listId);

  DragBoardPhase getPhase() const { return phase_; }
  bool isFloating() const { return phase_ == DragBoardPhase::Floating; }
  bool isDeciding() const { return phase_ == DragBoardPhase::AtHome || phase_ == DragBoardPhase::Floating; }
  bool isLanding() const { return phase_ == DragBoardPhase::Landing || phase_ == DragBoardPhase::Settling; }
  const std::string& getHomeId() const { return homeId_; }
  const std::string& getHeldKey() const { return heldKey_; }
  std::uint64_t getFlight() const { return flight_; }

  /*
   * The finger's offset to the held row's center at pickup.
   */
  DragPoint getGrab() const { return grab_; }

  /*
   * The list the floating row would drop into, or the list it is landing in. It is empty at
   * home, when idle and after the target left the board.
   */
  const std::string& getTargetId() const { return targetId_; }

private:
  void reset();

  DragBoardPhase phase_ = DragBoardPhase::Idle;
  std::string homeId_;
  std::string heldKey_;
  std::string targetId_;
  std::uint64_t flight_ = 0;
  DragPoint grab_;

  // The lift out: where the picture started and the finger then, and when.
  DragPoint liftFrom_;
  DragPoint liftFinger_;
  double liftAtMs_ = 0.0;
  bool liftPending_ = false;

  double landDeadlineMs_ = 0.0;
};

}
