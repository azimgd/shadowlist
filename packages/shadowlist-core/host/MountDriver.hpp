#pragma once

#include <shadowlist-core/host/MountedRange.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Mounted range settings, from the list's props.
 */
struct MountSettings {
  std::size_t overscanRows = DEFAULT_OVERSCAN_ROWS;
  std::size_t overscanRowsLeading = DEFAULT_OVERSCAN_ROWS_LEADING;
  std::size_t minimumStep = MOUNT_STEP_ROWS;
};

/*
 * What the renderer mounted in the commit the driver runs on, read from its props.
 */
struct MountedBasis {
  /*
   * The rows mounted around the screen. Rows a scroll command mounts next to them are not part
   * of it.
   */
  MountedRange range;

  /*
   * The sequence of the newest range the renderer applied, or 0 when its range is its own,
   * like the initial one. The next report is then the first.
   */
  std::uint64_t sequence = 0;

  /*
   * Goes up when the renderer rebuilds its range itself, like around a new scroll target.
   * Ranges sent for an older generation are dropped on both sides.
   */
  std::uint64_t generation = 0;

  /*
   * A scroll command's rows are mounted next to the range until a report says it landed.
   */
  bool seedPending = false;
};

/*
 * A range for the renderer to mount. It goes by its edge keys, which keeps the same rows across
 * a data change on the way. A landed message only says a report came while seedPending was set. It
 * has sequence 0 and no range.
 */
struct MountMessage {
  std::string lowKey;
  std::string highKey;
  bool lowAtStart = false;
  bool highAtEnd = false;
  std::uint64_t sequence = 0;
  std::uint64_t generation = 0;
  bool landed = false;
  MountedRange range;
};

/*
 * Moves the mounted range along with the measured rows for a host whose renderer mounts the
 * rows, like Fabric where React does. One range is in flight at a time. The driver sends it
 * and waits until a commit's basis carries its sequence. A slow renderer never gets steps
 * stacked into one commit. A report sends at once. The overscan beyond the measured rows grows
 * one step per frame toward the target: after each applied range the host asks for the next
 * step on its next display frame, see takeStep. Not thread safe.
 */
class MountDriver final {
public:
  using IndexOfKey = std::function<std::size_t(const std::string&)>;

  void setSettings(const MountSettings& settings);

  /*
   * The commit's basis. Call before its reports.
   */
  void setBasis(const MountedBasis& basis);

  /*
   * The measured rows changed.
   */
  void reportMeasured(const MountedRange& measured);

  /*
   * After the commit's core update, with its keys: the message to send now, if any. Also
   * prepares the next step when the range is not at its target yet.
   */
  std::optional<MountMessage> process(const std::vector<std::string>& keys, const IndexOfKey& indexOfKey);

  /*
   * Whether a step waits for the host's next frame.
   */
  bool hasStep() const {
    return !pending_ && step_.has_value();
  }

  /*
   * The prepared step, sent now. Nothing while a range is in flight.
   */
  std::optional<MountMessage> takeStep();

  /*
   * Whether a range is sent and not applied yet.
   */
  bool isPending() const {
    return pending_;
  }

private:
  /*
   * Where the last report wants the range to end up, and what was measured then, by key.
   */
  struct Target {
    std::string lowKey;
    std::string highKey;
    std::string measuredLowKey;
    std::string measuredHighKey;
  };

  MountMessage message(const std::vector<std::string>& keys, const MountedRange& range) const;
  std::optional<MountMessage> send(MountMessage message);
  MountedRange currentRange(std::size_t size) const;
  void prepareStep(const std::vector<std::string>& keys, const IndexOfKey& indexOfKey);

  MountSettings settings_;
  MountedBasis basis_;
  std::optional<MountedRange> report_;
  std::optional<MountedRange> previousMeasured_;
  std::optional<Target> target_;
  std::optional<MountMessage> step_;
  bool stepReachesTarget_ = false;
  std::uint64_t sequence_ = 0;
  std::uint64_t pendingSequence_ = 0;
  bool pending_ = false;
};

}
