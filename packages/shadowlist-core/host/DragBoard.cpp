#include <shadowlist-core/host/DragBoard.hpp>

#include <algorithm>
#include <utility>

namespace azimgd::shadowlist {

std::size_t dragBoardDropIndex(std::size_t insertionIndex, const std::string& insertionKey, std::size_t rowCount) {
  if (insertionIndex == UNDEFINED_INDEX) {
    return UNDEFINED_INDEX;
  }
  if (rowCount == UNDEFINED_INDEX) {
    return insertionIndex;
  }
  return insertionKey.empty() ? rowCount : std::min(insertionIndex, rowCount);
}

void DragBoard::begin(std::string homeId, std::string heldKey, DragPoint finger, DragPoint heldCenter) {
  phase_ = DragBoardPhase::AtHome;
  homeId_ = std::move(homeId);
  heldKey_ = std::move(heldKey);
  targetId_.clear();
  flight_ += 1;
  grab_ = {heldCenter.x - finger.x, heldCenter.y - finger.y};
  liftPending_ = false;
  landDeadlineMs_ = 0.0;
}

DragBoardStep DragBoard::hover(const std::vector<std::string>& candidates) {
  DragBoardStep step;
  if (!isDeciding()) {
    return step;
  }
  const std::string* hovered = nullptr;
  for (const std::string& id : candidates) {
    if (id.empty()) {
      continue;
    }
    if (id == homeId_) {
      hovered = &id;
      break;
    }
    if (!hovered) {
      hovered = &id;
    }
  }
  bool overHome = hovered && *hovered == homeId_;

  if (phase_ == DragBoardPhase::AtHome) {
    // At home the row lifts out only over another list. Between lists it stays in place.
    if (!hovered || overHome) {
      return step;
    }
    phase_ = DragBoardPhase::Floating;
    targetId_ = *hovered;
    step.kind = DragBoardStep::Kind::LiftOut;
    step.next = targetId_;
    return step;
  }

  if (overHome) {
    step.kind = DragBoardStep::Kind::ReturnHome;
    step.previous = std::move(targetId_);
    targetId_.clear();
    phase_ = DragBoardPhase::AtHome;
    return step;
  }
  // Between lists the last one hovered stays the target.
  if (!hovered || *hovered == targetId_) {
    return step;
  }
  step.kind = DragBoardStep::Kind::Retarget;
  step.previous = std::move(targetId_);
  step.next = *hovered;
  targetId_ = *hovered;
  return step;
}

DragBoardFrame DragBoard::frame(
  DragPoint finger,
  DragPoint shownCenter,
  const std::vector<std::string>& candidates,
  double nowMs) {
  DragBoardFrame result;
  result.step = hover(candidates);
  if (result.step.kind == DragBoardStep::Kind::LiftOut) {
    liftFrom_ = shownCenter;
    liftFinger_ = finger;
    liftAtMs_ = nowMs;
    liftPending_ = true;
  }
  result.floating = isFloating();
  result.hideHeld = phase_ != DragBoardPhase::Idle && phase_ != DragBoardPhase::AtHome;
  if (!result.floating) {
    return result;
  }
  DragPoint under{finger.x + grab_.x, finger.y + grab_.y};
  if (!liftPending_) {
    result.overlayCenter = under;
    return result;
  }
  /*
   * From where the row showed, moved along with the finger since the lift, to its place under
   * the finger, eased out.
   */
  double progress = DRAG_BOARD_LIFT_OUT_MS > 0.0 ? (nowMs - liftAtMs_) / DRAG_BOARD_LIFT_OUT_MS : 1.0;
  if (progress >= 1.0) {
    liftPending_ = false;
    result.overlayCenter = under;
    return result;
  }
  progress = std::max(0.0, progress);
  double eased = 1.0 - (1.0 - progress) * (1.0 - progress);
  DragPoint from{liftFrom_.x + finger.x - liftFinger_.x, liftFrom_.y + finger.y - liftFinger_.y};
  result.overlayCenter = {from.x + (under.x - from.x) * eased, from.y + (under.y - from.y) * eased};
  return result;
}

DragBoardRelease DragBoard::release(
  std::size_t insertionIndex,
  const std::string& insertionKey,
  std::size_t rowCount,
  double nowMs) {
  DragBoardRelease result;
  if (phase_ != DragBoardPhase::Floating) {
    reset();
    return result;
  }
  std::size_t index = dragBoardDropIndex(insertionIndex, insertionKey, rowCount);
  if (targetId_.empty() || heldKey_.empty() || index == UNDEFINED_INDEX) {
    result.kind = DragBoardRelease::Kind::Cancel;
    reset();
    return result;
  }
  result.kind = DragBoardRelease::Kind::Drop;
  result.listId = targetId_;
  result.index = index;
  result.key = insertionKey;
  phase_ = DragBoardPhase::Landing;
  landDeadlineMs_ = nowMs + DRAG_BOARD_LAND_TIMEOUT_MS;
  liftPending_ = false;
  return result;
}

bool DragBoard::landed(const std::string& listId, bool keysHoldHeldKey) {
  if (phase_ != DragBoardPhase::Landing || listId.empty() || listId != targetId_ || !keysHoldHeldKey) {
    return false;
  }
  phase_ = DragBoardPhase::Settling;
  return true;
}

bool DragBoard::landed(const std::string& listId, const std::vector<std::string>& keys) {
  return landed(listId, std::find(keys.begin(), keys.end(), heldKey_) != keys.end());
}

bool DragBoard::expired(double nowMs) {
  if (phase_ != DragBoardPhase::Landing || nowMs < landDeadlineMs_) {
    return false;
  }
  reset();
  return true;
}

std::string DragBoard::cancel() {
  std::string target = std::move(targetId_);
  reset();
  return target;
}

void DragBoard::leave(const std::string& listId) {
  if (phase_ == DragBoardPhase::Idle || listId.empty()) {
    return;
  }
  if (isLanding()) {
    // The home list's part ended with the drop. Only the list the row lands in matters now.
    if (listId == targetId_) {
      reset();
    }
    return;
  }
  if (listId == homeId_) {
    reset();
  } else if (listId == targetId_) {
    targetId_.clear();
  }
}

void DragBoard::reset() {
  phase_ = DragBoardPhase::Idle;
  homeId_.clear();
  heldKey_.clear();
  targetId_.clear();
  liftPending_ = false;
  landDeadlineMs_ = 0.0;
}

}
