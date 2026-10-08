#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Where a row shows on screen: its leading corner, the top left in both kits, in host units.
 */
struct ScreenPoint {
  double x = 0.0;
  double y = 0.0;
};

/*
 * How a row mounted after a change animates. Move slides a row that showed before from where
 * it was. Insert fades in a new row. Carry slides a row that had no place on screen before by
 * the shift of the row above it.
 */
enum class ChangeStepKind {
  Move,
  Insert,
  Carry,
};

/*
 * One row's animation. fromX and fromY are where the row showed before, relative to where it
 * shows now. Insert leaves them 0.
 */
struct ChangeStep {
  ChangeStepKind kind = ChangeStepKind::Move;
  double fromX = 0.0;
  double fromY = 0.0;
};

/*
 * What a list with animatesChanges animates. A data change records where every mounted row is
 * on screen. After the layout pass that applies it, rows that stay slide from there to their
 * new place, new rows fade in and removed rows fade out where they were. Several changes before
 * one layout add up and the first one records the screen. A key removed by one change and
 * inserted by another moved, and slides like any row that stays. The host owns the views and
 * the animations.
 */
class ChangeAnimation final {
public:
  /*
   * A change is about to reach the core. Returns whether it starts a new animation. The host
   * then records where every mounted row shows with recordPosition.
   */
  bool capture(const std::vector<std::string>& removed, const std::vector<std::string>& inserted);

  void recordPosition(const std::string& key, ScreenPoint position);

  bool isPending() const { return pending_; }

  /*
   * Where a removed row showed before the change, for its fade out, or nothing for a row that
   * does not fade out.
   */
  std::optional<ScreenPoint> removedPosition(const std::string& key) const;

  /*
   * The animation of every row mounted after the layout pass. keys and positions are the rows
   * low to high and where each one shows now. Ends the animation. Without a pending change the
   * result is empty.
   */
  std::vector<ChangeStep> run(const std::vector<std::string>& keys, const std::vector<ScreenPoint>& positions);

private:
  bool pending_ = false;
  std::unordered_map<std::string, ScreenPoint> before_;
  std::unordered_set<std::string> inserted_;
  std::unordered_set<std::string> removed_;
};

}
