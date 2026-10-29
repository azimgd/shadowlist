#include <shadowlist-core/host/MountDriver.hpp>

#include <algorithm>
#include <utility>

namespace azimgd::shadowlist {

void MountDriver::setSettings(const MountSettings& settings) {
  settings_ = settings;
}

void MountDriver::setBasis(const MountedBasis& basis) {
  if (basis.generation != basis_.generation) {
    /*
     * The renderer rebuilt its range. A range in flight was built on the old one and the
     * renderer drops it. A report held back for it would trim the new range to the old screen.
     */
    pending_ = false;
    target_.reset();
    step_.reset();
    report_.reset();
  } else if (pending_ && basis.sequence >= pendingSequence_) {
    pending_ = false;
  }
  basis_ = basis;
}

void MountDriver::reportMeasured(const MountedRange& measured) {
  report_ = measured;
}

MountedRange MountDriver::currentRange(std::size_t size) const {
  const MountedRange& range = basis_.range;
  if (range.isEmpty() || range.high >= size) {
    return {};
  }
  return range;
}

MountMessage MountDriver::message(const KeyList& keys, const MountedRange& range) const {
  MountMessage message;
  message.lowKey = keys[range.low];
  message.highKey = keys[range.high];
  message.lowAtStart = range.low == 0;
  message.highAtEnd = range.high == keys.size() - 1;
  message.generation = basis_.generation;
  message.range = range;
  return message;
}

std::optional<MountMessage> MountDriver::send(MountMessage message) {
  message.sequence = ++sequence_;
  pendingSequence_ = message.sequence;
  pending_ = true;
  step_.reset();
  return message;
}

std::optional<MountMessage> MountDriver::process(const KeyList& keys, const IndexOfKey& indexOfKey) {
  // A report waits until the renderer applied the range in flight. It is the newest by then.
  if (pending_) {
    return std::nullopt;
  }
  std::size_t size = keys.size();
  std::optional<MountMessage> sent;
  if (report_) {
    MountedRange measured = *report_;
    report_.reset();
    if (!measured.isEmpty() && measured.high < size) {
      std::optional<MountedRange> previousMeasured = previousMeasured_;
      previousMeasured_ = measured;
      MountedRange current = currentRange(size);
      auto reported = reportedMountedRange(current, measured, previousMeasured, basis_.sequence == 0, size,
        settings_.overscanRows, settings_.overscanRowsLeading, settings_.minimumStep);
      if (reported) {
        const MountedRange& target = reported->target;
        if (reported->range == target) {
          target_.reset();
        } else {
          target_ = Target{keys[target.low], keys[target.high], keys[measured.low], keys[measured.high]};
        }
        if (reported->range != current) {
          sent = send(message(keys, reported->range));
        }
      }
      if (!sent && basis_.seedPending) {
        MountMessage landed;
        landed.landed = true;
        landed.generation = basis_.generation;
        sent = landed;
      }
    }
  }
  if (!pending_) {
    prepareStep(keys, indexOfKey);
  }
  return sent;
}

/*
 * The next step toward the target from the applied range. Before the first report the range is
 * the renderer's own and takes no steps.
 */
void MountDriver::prepareStep(const KeyList& keys, const IndexOfKey& indexOfKey) {
  step_.reset();
  if (!target_ || basis_.sequence == 0) {
    return;
  }
  std::size_t low = indexOfKey(target_->lowKey);
  std::size_t high = indexOfKey(target_->highKey);
  std::size_t measuredLow = indexOfKey(target_->measuredLowKey);
  std::size_t measuredHigh = indexOfKey(target_->measuredHighKey);
  if (low == UNDEFINED_INDEX || high == UNDEFINED_INDEX || measuredLow == UNDEFINED_INDEX ||
      measuredHigh == UNDEFINED_INDEX || low > high) {
    target_.reset();
    return;
  }
  MountedRange current = currentRange(keys.size());
  MountedRange measured{std::min(measuredLow, measuredHigh), std::max(measuredLow, measuredHigh)};
  MountedRange target{low, high};
  MountedRange next = stepMountedRange(current, target, measured, mountStepForRange(measured, settings_.minimumStep));
  if (next == current) {
    if (next == target) {
      target_.reset();
    }
    return;
  }
  step_ = message(keys, next);
  stepReachesTarget_ = next == target;
}

std::optional<MountMessage> MountDriver::takeStep() {
  if (pending_ || !step_) {
    return std::nullopt;
  }
  if (stepReachesTarget_) {
    target_.reset();
  }
  MountMessage step = std::move(*step_);
  return send(std::move(step));
}

}
