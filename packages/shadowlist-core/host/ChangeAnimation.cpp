#include <shadowlist-core/host/ChangeAnimation.hpp>

namespace azimgd::shadowlist {

bool ChangeAnimation::capture(const std::vector<std::string>& deleted, const std::vector<std::string>& inserted) {
  bool started = !pending_;
  if (started) {
    before_.clear();
    deleted_.clear();
    inserted_.clear();
    pending_ = true;
  }
  std::unordered_set<std::string> insertedNow(inserted.begin(), inserted.end());
  std::unordered_set<std::string> deletedNow(deleted.begin(), deleted.end());
  // A key on both sides moved, in one change or across two. It slides like any row that stays.
  for (const std::string& key : deleted) {
    bool insertedBefore = inserted_.erase(key) > 0;
    if (insertedNow.count(key) == 0 && !insertedBefore) {
      deleted_.insert(key);
    }
  }
  for (const std::string& key : inserted) {
    bool deletedBefore = deleted_.erase(key) > 0;
    if (deletedNow.count(key) == 0 && !deletedBefore) {
      inserted_.insert(key);
    }
  }
  return started;
}

void ChangeAnimation::recordPosition(const std::string& key, ScreenPoint position) {
  before_[key] = position;
}

std::optional<ScreenPoint> ChangeAnimation::deletedPosition(const std::string& key) const {
  if (!pending_ || deleted_.count(key) == 0) {
    return std::nullopt;
  }
  auto previous = before_.find(key);
  if (previous == before_.end()) {
    return std::nullopt;
  }
  return previous->second;
}

std::vector<ChangeStep> ChangeAnimation::run(
  const std::vector<std::string>& keys,
  const std::vector<ScreenPoint>& positions) {
  std::vector<ChangeStep> steps;
  if (!pending_) {
    return steps;
  }
  pending_ = false;
  steps.reserve(keys.size());
  ChangeStep carried{ChangeStepKind::Carry, 0.0, 0.0};
  for (std::size_t at = 0; at < keys.size() && at < positions.size(); ++at) {
    auto previous = before_.find(keys[at]);
    if (previous != before_.end()) {
      ChangeStep move{ChangeStepKind::Move, previous->second.x - positions[at].x, previous->second.y - positions[at].y};
      carried.fromX = move.fromX;
      carried.fromY = move.fromY;
      steps.push_back(move);
    } else if (inserted_.count(keys[at]) > 0) {
      steps.push_back({ChangeStepKind::Insert, 0.0, 0.0});
    } else {
      // Came into view without a place on screen before. It moves with the row above it.
      steps.push_back(carried);
    }
  }
  before_.clear();
  inserted_.clear();
  deleted_.clear();
  return steps;
}

}
