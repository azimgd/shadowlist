#pragma once

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/ListLayout.hpp>
#include <shadowlist-core/host/MountedRange.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Core passes per layout. Each pass measures the rows its window brought in or settles a
 * correction.
 */
constexpr int MAX_CORE_PASSES_PER_LAYOUT = 4;

/*
 * Layouts in a row that may end with a correction still running. After that the driver
 * stops asking the host for another frame.
 */
constexpr int MAX_SETTLING_LAYOUTS = 60;

/*
 * Key edits with at most this many runs of adjacent rows are applied to the key list in
 * place, one vector insert or erase per run. More runs rebuild the list in one pass.
 */
constexpr std::size_t MAX_IN_PLACE_KEY_RUNS = 8;

/*
 * List settings that change rarely. The host sends them again when a property changes.
 */
struct ListSettings {
  double estimatedRowSize = 120.0;
  double overscan = 1.0;
  double startReachedThreshold = 1.0;
  double endReachedThreshold = 1.0;
  std::size_t numberOfColumns = 1;
  bool inverted = false;
  bool followAppends = false;
  bool horizontal = false;
  bool snapToItem = false;
  SnapAlignment snapAlignment = SnapAlignment::Start;
};

/*
 * What the host reports for one layout. Sizes are along and across the scroll axis, in the
 * host's units. The offset includes the header.
 */
struct PassInput {
  double offset = 0.0;
  double windowAlong = 0.0;
  double windowCross = 0.0;
  double headerSize = 0.0;
  double footerSize = 0.0;
  ScrollPhase phase = ScrollPhase::Idle;
  bool userScrolled = false;
  bool tracking = false;
};

/*
 * What the host applies after a layout: the content size first, then the offset.
 * With settling set, a correction waits for one more report on the next frame.
 */
struct PassResult {
  double offset = 0.0;
  double contentAlong = 0.0;
  OffsetBand band;
  bool settling = false;
  bool reachedStart = false;
  bool reachedEnd = false;
};

/*
 * A row's frame in the content.
 */
struct RowRect {
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
};

/*
 * The rows to mount for one offset. Low and high are the lowest and highest rows that
 * overlap the viewport plus padding, or UNDEFINED_INDEX. Sticky is the pinned section header,
 * or UNDEFINED_INDEX.
 */
struct MountPlan {
  std::size_t low = UNDEFINED_INDEX;
  std::size_t high = UNDEFINED_INDEX;
  std::size_t sticky = UNDEFINED_INDEX;
  double viewLow = 0.0;
  double viewHigh = 0.0;
};

/*
 * Where an animated scroll command lands when its animation ends. The animation only goes to
 * an estimate. The core then lands exactly. A gesture cancels it.
 */
struct ScrollLanding {
  enum class Target { None, Row, Start, End };
  Target target = Target::None;
  std::size_t index = 0;
  double viewPosition = 0.0;
};

/*
 * The core driven the way a native list needs it. One synchronous layout runs update,
 * measure and correct until the window is measured and any correction landed. The host view
 * only mounts what the layout leaves in the window. It holds no platform types and is used
 * from one thread.
 */
class ListDriver final {
public:
  /*
   * Measures a row along the scroll axis for the given cross size.
   */
  using MeasureRow = std::function<double(std::size_t index, const std::string& key, double crossSize)>;

  ListDriver();

  /*
   * The core's callbacks capture this driver. A copy or a move would leave them on the old one.
   */
  ListDriver(const ListDriver&) = delete;
  ListDriver& operator=(const ListDriver&) = delete;
  ListDriver(ListDriver&&) = delete;
  ListDriver& operator=(ListDriver&&) = delete;

  void setMeasureRow(MeasureRow measureRow);
  void setSettings(const ListSettings& settings);

  /*
   * Rows pinned once scrolled past, like section headers. Sorted here.
   */
  void setStickyIndices(std::vector<std::size_t> sticky);
  bool hasSticky() const { return !sticky_.empty(); }

  void setKeys(std::vector<std::string> keys);

  /*
   * The full key list read again, like reloadData. Only the keys between the unchanged rows
   * at both ends are replaced. An edit at one end then reaches the core as one.
   */
  void reloadKeys(std::vector<std::string> keys);

  /*
   * Replace count keys from start with keys, like a host that compared the new data with
   * the old and kept the unchanged rows at both ends. Out of range parts are clipped.
   */
  void replaceKeys(std::size_t start, std::size_t count, std::vector<std::string> keys);

  /*
   * Rows inserted at these positions of the new data, with their keys in the same order.
   * Indices are sorted and deduplicated here. Ones past the end go at the end.
   */
  void insertKeys(std::vector<std::size_t> indices, std::vector<std::string> keys);

  /*
   * Rows deleted at these positions of the old data. Unsorted, repeated or out of range
   * indices are fine.
   */
  void deleteKeys(std::vector<std::size_t> indices);

  /*
   * Rows whose content changed under the same key. They are measured again when they are next
   * in the window.
   */
  void markRemeasure(const std::vector<std::size_t>& indices);

  std::size_t getKeyCount() const { return keys_.size(); }
  const std::vector<std::string>& getKeys() const { return keys_; }
  const std::string& getKeyAt(std::size_t index) const { return keys_[index]; }

  /*
   * Run the core until the window is measured and any correction landed.
   */
  PassResult runPasses(const PassInput& input);

  /*
   * Drop all sizes after a cross size change and hold the first visible row.
   */
  void resetKeepingPosition();

  /*
   * Rows the core placed, which lags getKeyCount until the next layout.
   */
  std::size_t getRowCount() const;
  std::uint64_t getGeometryVersion() const;
  double getContentAlong() const;

  /*
   * The rows the core keeps measured, or nothing before the first layout.
   */
  std::optional<MountedRange> getMeasuredRange() const;

  /*
   * The rows overlapping the viewport, or nothing before the first layout.
   */
  std::optional<MountedRange> getVisibleRange() const;

  RowRect getRowRect(std::size_t index) const;
  double getLeadingAt(std::size_t index) const;
  double getExtentAt(std::size_t index) const;
  double getFooterStart(double footerSize) const;

  /*
   * The row with this key, or UNDEFINED_INDEX.
   */
  std::size_t indexOfKey(const std::string& key) const;

  /*
   * The rows to mount for this offset, padded by pad on both ends. Insets are the parts of the
   * view before and after the window, like the area under translucent bars or padding with
   * clipping off. Rows there show and are mounted too.
   */
  MountPlan planMount(
    double offset,
    double windowAlong,
    double pad,
    double leadingInset = 0.0,
    double trailingInset = 0.0) const;

  /*
   * Whether a row in the plan's range gets mounted. In a masonry grid a row inside the index
   * range can be out of view.
   */
  bool shouldMount(const MountPlan& plan, std::size_t index) const;

  /*
   * The section header pinned for an offset, the last sticky row starting at or above it, or
   * UNDEFINED_INDEX.
   */
  std::size_t activeStickyIndex(double offset) const;

  /*
   * Where the pinned header's leading edge goes for an offset, pushed up by the next header.
   */
  double stickyLeading(std::size_t active, double offset) const;

  /*
   * Leading edge and extent of every sticky row, two values each, for a host that pins
   * headers without calling in on every frame. A row the core has not placed yet has an
   * infinite leading edge, the same as activeStickyIndex sorts it.
   */
  void stickyFrames(std::vector<double>& out) const;

  void scrollToRow(std::size_t index, double viewPosition, double viewOffset = 0.0);
  void scrollToStart();
  void scrollToEnd();
  double nearestSnapOffset(double target) const;

  /*
   * The row at the viewport start for an offset, or nothing before the first layout.
   */
  std::optional<AnchorState> getAnchorState(double offset) const;

  /*
   * Scroll the anchor's row back to where it sat. Returns false when no row has its key yet.
   */
  bool restoreAnchorState(const AnchorState& anchor);

  /*
   * Rows of the measured window that are not mounted get prefetched once, the core's window
   * running an overscan ahead of the viewport. A prefetched row that leaves the window before
   * it was mounted is cancelled. Mounted is the range the host mounts, rows given as indices.
   * Results go into prefetch and cancel, low to high.
   */
  void updatePrefetch(
    std::size_t mountedLow,
    std::size_t mountedHigh,
    std::vector<std::size_t>& prefetch,
    std::vector<std::size_t>& cancel);

  /*
   * Where an animated scroll to a row aims: its estimated position, inside 0 and maxOffset.
   */
  double animatedTargetOffset(std::size_t index, double viewPosition, double windowAlong, double maxOffset) const;

  void setLanding(const ScrollLanding& landing) { landing_ = landing; }
  void cancelLanding() { landing_ = ScrollLanding{}; }

  /*
   * Send the landing's command to the core after the animation ended. Returns whether there
   * was one.
   */
  bool land();

  /*
   * Pick up the row at index. Touches are in the content, along and across the scroll axis.
   */
  void dragBegin(std::size_t index, double touchAlong, double touchCross);
  void dragEnd();
  bool isDragging() const { return !heldKey_.empty(); }

  /*
   * Where the held row is in the data now, or UNDEFINED_INDEX without a drag or when its key is gone.
   */
  std::size_t getHeldIndex() const;

  /*
   * Where the held row at index held shows, as a translation from its resting frame. The drag's
   * origin follows the held row to its index in the data now.
   */
  DragOffset placeHeld(std::size_t held, double touchAlong, double touchCross);

  /*
   * Find the drop slot among the mounted rows, given by index in any order. The held row is
   * skipped.
   */
  void dragUpdateInsertion(const std::vector<std::size_t>& mounted);

  DragOffset dragShiftFor(std::size_t index) const { return drag_.offsetFor(index); }
  std::size_t getDragOriginIndex() const { return drag_.getOriginIndex(); }
  std::size_t getDragInsertionIndex() const { return drag_.getInsertionIndex(); }

private:
  void installCallbacks();
  void recordEdit(KeyEdit edit);
  KeyEdit endEdit(std::size_t start, std::size_t deleted, std::size_t inserted) const;
  void insertKeysInPlace(std::vector<std::pair<std::size_t, std::string>>& inserted);
  void insertKeysRebuilding(std::vector<std::pair<std::size_t, std::string>>& inserted);
  FrameInput makeFrameInput(const PassInput& input, double offset, bool firstPass) const;
  bool measureWindow(double offset);
  void measureIndex(std::size_t index, SizeBatch& batch);
  PassResult finishPasses(double offset);
  double clampOffset(double target, const PassInput& input) const;
  bool overlaps(std::size_t index, double viewLow, double viewHigh) const;
  DragRow dragRowAt(std::size_t index) const;

  std::unique_ptr<Container> core_;
  MeasureRow measureRow_;
  ListSettings settings_;
  std::vector<std::string> keys_;
  std::vector<std::size_t> sticky_;
  std::unordered_set<std::string> remeasureKeys_;
  std::unordered_set<std::string> prefetched_;
  ScrollLanding landing_;
  DragReorder drag_;
  std::string heldKey_;
  std::uint64_t echoToken_ = 0;
  double windowCross_ = 0.0;
  bool keysChanged_ = true;

  /*
   * The one edit at an end of the keys since the core last took them, which lets the core
   * skip comparing every key. Unknown after any other change or a second edit.
   */
  KeyEdit pendingEdit_;
  int editsSinceUpdate_ = 0;
  bool reachedStart_ = false;
  bool reachedEnd_ = false;
  int settlingLayouts_ = 0;
};

}
