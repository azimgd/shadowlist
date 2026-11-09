#pragma once

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

/*
 * What the kits' JNI bridge (shadowlist-android) and wasm bridge (shadowlist-wasm) share: the
 * pass slots, the commit flags and the conversions of their numbers into the driver's types.
 * Both bridges only marshal arrays in and out of these. ShadowListKitCore.kt and
 * ShadowListKitCore.ts mirror the slot numbers.
 */
namespace azimgd::shadowlist::kit {

/*
 * Slots of a pass's values. The first block is what the view reports, the second what it applies.
 */
enum PassSlot : int {
  PASS_OFFSET = 0,
  PASS_WINDOW_ALONG,
  PASS_WINDOW_CROSS,
  PASS_HEADER_SIZE,
  PASS_FOOTER_SIZE,
  PASS_PHASE,
  PASS_USER_SCROLLED,
  PASS_TRACKING,
  PASS_PREFETCH,
  PASS_MOUNT_PAD,
  PASS_LEADING_INSET,
  PASS_TRAILING_INSET,
  PASS_OUT_OFFSET,
  PASS_OUT_CONTENT,
  PASS_OUT_BAND_LOW,
  PASS_OUT_BAND_HIGH,
  PASS_OUT_SETTLING,
  PASS_OUT_REACHED_START,
  PASS_OUT_REACHED_END,
  PASS_OUT_GEOMETRY,
  PASS_OUT_MEASURED_LOW,
  PASS_OUT_MEASURED_HIGH,
  PASS_OUT_FOOTER_START,
  PASS_SLOTS,
};

static_assert(PASS_SLOTS == 23, "ShadowListKitCore.kt and ShadowListKitCore.ts mirror the pass slots");

/*
 * Bits of a key commit's flags: whether it diffs the items, records their content versions or
 * compares them against the ones recorded.
 */
enum CommitFlag : int {
  COMMIT_DIFF = 1,
  COMMIT_RECORD_VERSIONS = 2,
  COMMIT_UPDATE_VERSIONS = 4,
};

/*
 * The bridges number the core's enums in declaration order. These hold the order fixed.
 */
static_assert(static_cast<int>(ScrollPhase::Idle) == 0 && static_cast<int>(ScrollPhase::Dragging) == 1 &&
  static_cast<int>(ScrollPhase::Settling) == 2);
static_assert(static_cast<int>(ScrollLanding::Target::None) == 0 && static_cast<int>(ScrollLanding::Target::Row) == 1 &&
  static_cast<int>(ScrollLanding::Target::Start) == 2 && static_cast<int>(ScrollLanding::Target::End) == 3);
static_assert(static_cast<int>(SnapAlignment::Start) == 0 && static_cast<int>(SnapAlignment::Center) == 1 &&
  static_cast<int>(SnapAlignment::End) == 2);

/*
 * A bridge's index as the core's: a negative one is UNDEFINED_INDEX.
 */
inline std::size_t indexFrom(std::int64_t value) {
  return value < 0 ? UNDEFINED_INDEX : static_cast<std::size_t>(value);
}

/*
 * The core's index as a bridge's: UNDEFINED_INDEX is -1.
 */
inline std::int32_t intFrom(std::size_t index) {
  return index == UNDEFINED_INDEX ? -1 : static_cast<std::int32_t>(index);
}

/*
 * A scroll phase number, Idle for one out of range.
 */
inline ScrollPhase scrollPhaseFrom(double value) {
  if (value == 1.0) {
    return ScrollPhase::Dragging;
  }
  if (value == 2.0) {
    return ScrollPhase::Settling;
  }
  return ScrollPhase::Idle;
}

/*
 * A snap alignment number, Start for one out of range.
 */
inline SnapAlignment snapAlignmentFrom(std::int32_t value) {
  return value == 1 ? SnapAlignment::Center : value == 2 ? SnapAlignment::End : SnapAlignment::Start;
}

/*
 * A landing for an animated command: target is a ScrollLanding::Target number, None for one out
 * of range, and a negative index lands on row 0.
 */
inline ScrollLanding landingFrom(std::int32_t target, std::int32_t index, double viewPosition) {
  ScrollLanding landing;
  if (target >= 1 && target <= 3) {
    landing.target = static_cast<ScrollLanding::Target>(target);
  }
  landing.index = static_cast<std::size_t>(index < 0 ? 0 : index);
  landing.viewPosition = viewPosition;
  return landing;
}

/*
 * The settings a kit sends, a column count below one counting as one.
 */
inline ListSettings listSettingsFrom(
  double estimatedRowSize,
  double overscan,
  double startReachedThreshold,
  double endReachedThreshold,
  std::int32_t numberOfColumns,
  bool inverted,
  bool followAppends,
  bool horizontal,
  bool snapToItem,
  std::int32_t snapAlignment) {
  ListSettings settings;
  settings.estimatedRowSize = estimatedRowSize;
  settings.overscan = overscan;
  settings.startReachedThreshold = startReachedThreshold;
  settings.endReachedThreshold = endReachedThreshold;
  settings.numberOfColumns = static_cast<std::size_t>(numberOfColumns < 1 ? 1 : numberOfColumns);
  settings.inverted = inverted;
  settings.followAppends = followAppends;
  settings.horizontal = horizontal;
  settings.snapToItem = snapToItem;
  settings.snapAlignment = snapAlignmentFrom(snapAlignment);
  return settings;
}

inline PassInput readPassInput(const double* slots) {
  PassInput input;
  input.offset = slots[PASS_OFFSET];
  input.windowAlong = slots[PASS_WINDOW_ALONG];
  input.windowCross = slots[PASS_WINDOW_CROSS];
  input.headerSize = slots[PASS_HEADER_SIZE];
  input.footerSize = slots[PASS_FOOTER_SIZE];
  input.phase = scrollPhaseFrom(slots[PASS_PHASE]);
  input.userScrolled = slots[PASS_USER_SCROLLED] != 0.0;
  input.tracking = slots[PASS_TRACKING] != 0.0;
  input.prefetch = slots[PASS_PREFETCH] != 0.0;
  input.mountPad = slots[PASS_MOUNT_PAD];
  input.leadingInset = slots[PASS_LEADING_INSET];
  input.trailingInset = slots[PASS_TRAILING_INSET];
  return input;
}

inline void writePassResult(const ListDriver& driver, const PassResult& result, double* slots) {
  slots[PASS_OUT_OFFSET] = result.offset;
  slots[PASS_OUT_CONTENT] = result.contentAlong;
  slots[PASS_OUT_BAND_LOW] = result.band.low;
  slots[PASS_OUT_BAND_HIGH] = result.band.high;
  slots[PASS_OUT_SETTLING] = result.settling ? 1.0 : 0.0;
  slots[PASS_OUT_REACHED_START] = result.reachedStart ? 1.0 : 0.0;
  slots[PASS_OUT_REACHED_END] = result.reachedEnd ? 1.0 : 0.0;
  slots[PASS_OUT_GEOMETRY] = static_cast<double>(driver.getGeometryVersion());
  std::optional<MountedRange> measured = driver.getMeasuredRange();
  slots[PASS_OUT_MEASURED_LOW] = measured ? static_cast<double>(measured->low) : -1.0;
  slots[PASS_OUT_MEASURED_HIGH] = measured ? static_cast<double>(measured->high) : -1.0;
  slots[PASS_OUT_FOOTER_START] = result.footerStart;
}

inline bool validRow(const ListDriver& driver, std::int64_t index) {
  return index >= 0 && static_cast<std::size_t>(index) < driver.getRowCount();
}

inline void writeRect(const RowRect& rect, double* out) {
  out[0] = rect.x;
  out[1] = rect.y;
  out[2] = rect.width;
  out[3] = rect.height;
}

/*
 * Frames of rows low to high into out, four values each, x y width height. Returns how many
 * values it wrote, 0 for a range outside the rows.
 */
inline std::size_t copyRowRects(const ListDriver& driver, std::int64_t low, std::int64_t high, double* out) {
  if (!validRow(driver, low) || !validRow(driver, high) || high < low) {
    return 0;
  }
  for (std::int64_t index = low; index <= high; ++index) {
    writeRect(driver.getRowRect(static_cast<std::size_t>(index)), out + static_cast<std::size_t>(index - low) * 4);
  }
  return static_cast<std::size_t>(high - low + 1) * 4;
}

/*
 * A key commit's result for a bridge, packed: the delete count and previous indices, the insert
 * count and next indices, the move count and from, to pairs, then the count and items whose
 * content version changed.
 */
inline std::vector<std::int32_t> packCommit(const KeyDiff& diff, const std::vector<std::size_t>& reloaded) {
  std::vector<std::int32_t> packed;
  packed.reserve(4 + diff.deleted.size() + diff.inserted.size() + diff.moved.size() * 2 + reloaded.size());
  packed.push_back(static_cast<std::int32_t>(diff.deleted.size()));
  for (std::size_t index : diff.deleted) {
    packed.push_back(static_cast<std::int32_t>(index));
  }
  packed.push_back(static_cast<std::int32_t>(diff.inserted.size()));
  for (std::size_t index : diff.inserted) {
    packed.push_back(static_cast<std::int32_t>(index));
  }
  packed.push_back(static_cast<std::int32_t>(diff.moved.size()));
  for (const KeyMove& move : diff.moved) {
    packed.push_back(static_cast<std::int32_t>(move.from));
    packed.push_back(static_cast<std::int32_t>(move.to));
  }
  packed.push_back(static_cast<std::int32_t>(reloaded.size()));
  for (std::size_t item : reloaded) {
    packed.push_back(static_cast<std::int32_t>(item));
  }
  return packed;
}

}
