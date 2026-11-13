#include <shadowlist-core/host/ListDriver.hpp>

#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/RowKeyStore.hpp>
#include <shadowlist-core/host/Snap.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numeric>

namespace azimgd::shadowlist {

namespace {

/*
 * How many times a pass with prefetch on measures the rows its mount plan reaches before it
 * prefetches the rest.
 */
constexpr int MAX_MOUNT_PLAN_ROUNDS = 3;

/*
 * Runs of adjacent values in a sorted list of count values read through valueAt.
 */
template <typename ValueAt>
std::size_t countRuns(std::size_t count, ValueAt valueAt) {
  std::size_t runs = count > 0 ? 1 : 0;
  for (std::size_t at = 1; at < count; ++at) {
    if (valueAt(at) != valueAt(at - 1) + 1) {
      ++runs;
    }
  }
  return runs;
}

}

ListDriver::ListDriver() : core_(std::make_unique<Container>()) {
  installCallbacks();
}

void ListDriver::installCallbacks() {
  core_->onStartReachedCallback = [this]() { reachedStart_ = true; };
  core_->onEndReachedCallback = [this]() { reachedEnd_ = true; };
}

void ListDriver::setMeasureRow(MeasureRow measureRow) {
  measureRow_ = std::move(measureRow);
}

void ListDriver::setMeasureRows(MeasureRows measureRows) {
  measureRows_ = std::move(measureRows);
}

void ListDriver::setPrefetchRows(PrefetchRows prefetchRows) {
  prefetchRows_ = std::move(prefetchRows);
}

void ListDriver::setSettings(const ListSettings& settings) {
  settings_ = settings;
  settings_.numberOfColumns = std::max<std::size_t>(1, settings.numberOfColumns);
}

void ListDriver::setStickyIndices(std::vector<std::size_t> sticky) {
  std::sort(sticky.begin(), sticky.end());
  sticky.erase(std::unique(sticky.begin(), sticky.end()), sticky.end());
  sticky_ = std::move(sticky);
}

void ListDriver::setNonAnchorKeys(std::vector<std::string> keys) {
  nonAnchorKeys_ = std::move(keys);
  nonAnchorKeysChanged_ = true;
}

void ListDriver::setKeys(std::vector<std::string> keys) {
  keys_ = std::move(keys);
  recordEdit({});
}

/*
 * Keep the edit for the core when it is the only one before the next update.
 */
void ListDriver::recordEdit(KeyEdit edit) {
  pendingEdit_ = editsSinceUpdate_ == 0 ? edit : KeyEdit{};
  ++editsSinceUpdate_;
  keysChanged_ = true;
}

void ListDriver::reloadKeys(std::vector<std::string> keys) {
  KeySplice splice = keySplice(keys_, keys);
  reloadKeys(std::move(keys), splice);
}

void ListDriver::reloadKeys(std::vector<std::string> keys, const KeySplice& splice) {
  if (splice.isEmpty()) {
    return;
  }
  auto first = keys.begin() + static_cast<std::ptrdiff_t>(splice.start);
  std::vector<std::string> middle(std::make_move_iterator(first),
    std::make_move_iterator(first + static_cast<std::ptrdiff_t>(splice.inserted)));
  replaceKeys(splice.start, splice.deleted, std::move(middle));
}

void ListDriver::replaceKeys(std::size_t start, std::size_t count, std::vector<std::string> keys) {
  start = std::min(start, keys_.size());
  count = std::min(count, keys_.size() - start);
  if (count == 0 && keys.empty()) {
    return;
  }
  KeyEdit edit = keyEditOf(keys_.size(), start, count, keys.size());
  auto first = keys_.begin() + static_cast<std::ptrdiff_t>(start);
  std::size_t common = std::min(count, keys.size());
  std::move(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(common), first);
  if (count > common) {
    keys_.erase(first + static_cast<std::ptrdiff_t>(common), first + static_cast<std::ptrdiff_t>(count));
  } else if (keys.size() > common) {
    keys_.insert(first + static_cast<std::ptrdiff_t>(common), std::make_move_iterator(keys.begin() + static_cast<std::ptrdiff_t>(common)),
      std::make_move_iterator(keys.end()));
  }
  recordEdit(edit);
}

void ListDriver::insertKeys(std::vector<std::size_t> indices, std::vector<std::string> keys) {
  std::size_t count = std::min(indices.size(), keys.size());
  indices.resize(count);
  keys.resize(count);
  bool ascending = true;
  for (std::size_t position = 1; position < count && ascending; ++position) {
    ascending = indices[position - 1] < indices[position];
  }
  // Sort the indices with their keys, then keep the first of any repeated index.
  if (!ascending) {
    std::vector<std::size_t> order(count);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return indices[a] < indices[b]; });
    std::vector<std::size_t> sortedIndices;
    std::vector<std::string> sortedKeys;
    sortedIndices.reserve(count);
    sortedKeys.reserve(count);
    for (std::size_t position : order) {
      if (!sortedIndices.empty() && sortedIndices.back() == indices[position]) {
        continue;
      }
      sortedIndices.push_back(indices[position]);
      sortedKeys.push_back(std::move(keys[position]));
    }
    indices = std::move(sortedIndices);
    keys = std::move(sortedKeys);
  }
  if (indices.empty()) {
    return;
  }
  std::size_t previousCount = keys_.size();
  std::size_t runs = countRuns(indices.size(), [&](std::size_t at) { return indices[at]; });
  KeyEdit edit;
  if (runs == 1) {
    edit = keyEditOf(previousCount, std::min(indices.front(), previousCount), 0, indices.size());
  }
  if (runs <= MAX_IN_PLACE_KEY_RUNS) {
    insertKeysInPlace(indices, keys);
  } else {
    insertKeysRebuilding(indices, keys);
  }
  recordEdit(edit);
}

/*
 * Insert each run of adjacent new rows with one vector insert. Runs go in ascending order,
 * which leaves every row before the next run at its final index.
 */
void ListDriver::insertKeysInPlace(const std::vector<std::size_t>& indices, std::vector<std::string>& keys) {
  std::size_t first = 0;
  while (first < indices.size()) {
    std::size_t last = first + 1;
    while (last < indices.size() && indices[last] == indices[last - 1] + 1) {
      ++last;
    }
    std::size_t at = std::min(indices[first], keys_.size());
    keys_.insert(keys_.begin() + static_cast<std::ptrdiff_t>(at),
      std::make_move_iterator(keys.begin() + static_cast<std::ptrdiff_t>(first)),
      std::make_move_iterator(keys.begin() + static_cast<std::ptrdiff_t>(last)));
    first = last;
  }
}

/*
 * Build the new key list in one pass, for inserts spread over many places.
 */
void ListDriver::insertKeysRebuilding(const std::vector<std::size_t>& indices, std::vector<std::string>& keys) {
  std::vector<std::string> next;
  next.reserve(keys_.size() + indices.size());
  std::size_t source = 0;
  for (std::size_t position = 0; position < indices.size(); ++position) {
    // Keep the old keys that sit before this new row.
    while (next.size() < indices[position] && source < keys_.size()) {
      next.push_back(std::move(keys_[source++]));
    }
    next.push_back(std::move(keys[position]));
  }
  while (source < keys_.size()) {
    next.push_back(std::move(keys_[source++]));
  }
  keys_ = std::move(next);
}

void ListDriver::deleteKeys(std::vector<std::size_t> indices) {
  std::vector<std::size_t> deleted = deletionIndices(std::move(indices), keys_.size());
  if (deleted.empty()) {
    return;
  }
  std::size_t runs = countRuns(deleted.size(), [&](std::size_t at) { return deleted[at]; });
  KeyEdit edit;
  if (runs == 1) {
    edit = keyEditOf(keys_.size(), deleted.front(), deleted.size(), 0);
  }
  if (runs <= MAX_IN_PLACE_KEY_RUNS) {
    // Erase runs from the last one back, which keeps the earlier indices valid.
    std::size_t last = deleted.size();
    while (last > 0) {
      std::size_t first = last - 1;
      while (first > 0 && deleted[first - 1] + 1 == deleted[first]) {
        --first;
      }
      keys_.erase(keys_.begin() + static_cast<std::ptrdiff_t>(deleted[first]),
        keys_.begin() + static_cast<std::ptrdiff_t>(deleted[last - 1] + 1));
      last = first;
    }
  } else {
    std::vector<std::string> next;
    next.reserve(keys_.size() - deleted.size());
    std::size_t position = 0;
    for (std::size_t index = 0; index < keys_.size(); ++index) {
      if (position < deleted.size() && deleted[position] == index) {
        ++position;
        continue;
      }
      next.push_back(std::move(keys_[index]));
    }
    keys_ = std::move(next);
  }
  recordEdit(edit);
}

void ListDriver::markRemeasure(const std::vector<std::size_t>& indices) {
  for (std::size_t index : indices) {
    if (index < keys_.size()) {
      // A content edit starts a new hold at the row now on screen. Late first sizes do not.
      core_->state.clampedAnchor.reset();
      remeasureKeys_.insert(keys_[index]);
    }
  }
}

FrameInput ListDriver::makeFrameInput(const PassInput& input, double offset, bool userScrolled) const {
  FrameInput frame;
  frame.keysRef = keys_;
  frame.keysUnchanged = !keysChanged_;
  frame.keyEdit = keysChanged_ ? pendingEdit_ : KeyEdit{};
  double cellCross = input.windowCross / static_cast<double>(settings_.numberOfColumns);
  if (settings_.horizontal) {
    frame.offsetX = offset;
    frame.windowWidth = input.windowAlong;
    frame.windowHeight = input.windowCross;
    frame.estimatedRowSize = {settings_.estimatedRowSize, cellCross};
  } else {
    frame.offsetY = offset;
    frame.windowWidth = input.windowCross;
    frame.windowHeight = input.windowAlong;
    frame.estimatedRowSize = {cellCross, settings_.estimatedRowSize};
  }
  frame.headerSize = input.headerSize + (showsEmpty() ? input.emptySize : 0.0);
  frame.footerSize = input.footerSize;
  frame.nonAnchorKeysRef = &nonAnchorKeys_;
  frame.nonAnchorKeysUnchanged = !nonAnchorKeysChanged_;
  frame.inverted = settings_.inverted;
  frame.followAppends = settings_.followAppends;
  frame.horizontal = settings_.horizontal;
  frame.numberOfColumns = settings_.numberOfColumns;
  frame.overscan = settings_.overscan;
  frame.startReachedThreshold = settings_.startReachedThreshold;
  frame.endReachedThreshold = settings_.endReachedThreshold;
  frame.snapToItem = settings_.snapToItem;
  frame.snapAlignment = settings_.snapAlignment;
  frame.scrollPhase = input.phase;
  frame.userScrolled = userScrolled;
  frame.commitToken = echoToken_;
  frame.commitRevision = echoRevision_;
  frame.dropHint = dropHint_;
  return frame;
}

PassResult ListDriver::runPasses(const PassInput& input) {
  windowCross_ = input.windowCross;
  if (keysChanged_) {
    prefetchStale_ = true;
  }
  /*
   * A finger takes the list from a command on its way. So does any other move by the user once
   * the core runs the command.
   */
  bool commandRuns = core_->state.scrollToRowTarget == UNDEFINED_INDEX;
  if (input.phase == ScrollPhase::Dragging || (input.userScrolled && commandRuns)) {
    rowAim_.reset();
  }
  double offset = runCore(input, input.offset, input.userScrolled);
  /*
   * The passes left the list outside its content. It comes back to the end and the core runs
   * again there, as after a move that is not the user's. The rows that run measures can move the
   * end once more. The host then draws at the new end and runs again on the next frame.
   */
  bool movedAgain = false;
  double resting = restingOffset(offset, input);
  if (resting != offset) {
    offset = runCore(input, resting, false);
    resting = restingOffset(offset, input);
    movedAgain = resting != offset;
    offset = resting;
  }
  PassResult result = finishPasses(offset, input);
  result.settling = result.settling || movedAgain;
  return result;
}

/*
 * Update, measure and correct from offset until nothing moves. Returns where the list ends up.
 * userScrolled reaches the core with the first pass only.
 */
double ListDriver::runCore(const PassInput& input, double offset, bool userScrolled) {
  Container& core = *core_;
  bool horizontal = settings_.horizontal;
  int correctionPasses = 0;
  for (bool firstPass = true; correctionPasses < MAX_CORE_PASSES_PER_LAYOUT; firstPass = false) {
    Virtualizer::update(core, makeFrameInput(input, offset, firstPass && userScrolled));
    dropHint_ = DropHint{};
    keysChanged_ = false;
    nonAnchorKeysChanged_ = false;
    pendingEdit_ = KeyEdit{};
    editsSinceUpdate_ = 0;
    bool measured = measureRange(offset, input);

    ContainerStateUpdate update = core.resolveStateUpdate(horizontal ? offset : 0, horizontal ? 0 : offset,
      core.revision.contentWidth, core.revision.contentHeight);
    bool wrote = false;
    if (update.applyOffset) {
      double target = clampOffset(horizontal ? update.offsetX : update.offsetY, input);
      echoToken_ = update.commitToken;
      echoRevision_ = update.commitRevision;
      if (std::fabs(target - offset) >= 0.01) {
        offset = target;
        wrote = true;
      }
    }
    /*
     * The core counts a scroll to a row as arrived on the sizes it had when the pass began. The
     * rows the pass then measured, or the row it holds next, can leave the list off the target.
     * The command goes out again until the list rests on it.
     */
    if (rowAim_ && !core.hasPendingCommand()) {
      std::size_t index = core.indexOfKey(rowAim_->key);
      double maxOffset = std::max(0.0, getContentAlong() - input.windowAlong);
      double aimed = rowTargetOffset(core, index, rowAim_->viewPosition, rowAim_->viewOffset, input.windowAlong,
        maxOffset);
      bool landed = !measured && std::fabs(aimed - offset) < OFFSET_ARRIVED_THRESHOLD;
      if (index == UNDEFINED_INDEX || landed) {
        rowAim_.reset();
      } else {
        core.scrollToRow(index, rowAim_->viewPosition, rowAim_->viewOffset);
      }
    }
    /*
     * A move by the user reported at rest reaches the core as a gesture frame. One more pass
     * tells it the list rests.
     */
    bool gestureAtRest = input.phase == ScrollPhase::Idle && core.state.gestureActive;
    if (!measured && !wrote && !gestureAtRest && !core.hasPendingCommand()) {
      break;
    }
    // A pass that measured is one of a few: each row near the viewport is measured once.
    if (!measured) {
      ++correctionPasses;
    }
  }
  return offset;
}

/*
 * Pack the result and hand over the reached flags the passes raised.
 */
PassResult ListDriver::finishPasses(double offset, const PassInput& input) {
  const Container& core = *core_;
  PassResult result;
  result.offset = offset;
  result.contentAlong = getContentAlong();
  result.footerStart = core.getFooterStart(input.footerSize);
  result.band = core.computeOffsetBand();
  /*
   * The core's band knows nothing of the pinned header. The passes measured the one pinned at
   * this offset, and the band ends where another row pins.
   */
  if (!sticky_.empty() && !result.band.isEmpty()) {
    std::size_t active = activeStickyIndex(offset);
    auto next = active == UNDEFINED_INDEX ? sticky_.begin() : std::upper_bound(sticky_.begin(), sticky_.end(), active);
    if (active != UNDEFINED_INDEX) {
      result.band.low = std::max(result.band.low, getLeadingAt(active));
    }
    if (next != sticky_.end() && *next < getRowCount()) {
      result.band.high = std::min(result.band.high, getLeadingAt(*next) - OFFSET_BAND_MARGIN);
    }
  }
  settlingLayouts_ = core.hasPendingCommand() ? settlingLayouts_ + 1 : 0;
  result.settling = settlingLayouts_ > 0 && settlingLayouts_ < MAX_SETTLING_LAYOUTS;
  result.reachedStart = reachedStart_;
  result.reachedEnd = reachedEnd_;
  reachedStart_ = false;
  reachedEnd_ = false;
  result.atEnd = isAtEnd();
  result.showsEmpty = showsEmpty();
  return result;
}

/*
 * A finger holding the list past an edge keeps it there. Otherwise stay inside the range.
 * The range uses the content size the core just computed, which the host has not applied yet.
 */
double ListDriver::clampOffset(double target, const PassInput& input) const {
  if (input.tracking) {
    return target;
  }
  double maxOffset = std::max(0.0, getContentAlong() - input.windowAlong);
  return std::min(std::max(target, 0.0), maxOffset);
}

/*
 * Where a list left outside its content comes back to: the end the host has the driver hold, or
 * the offset itself. The range uses the content size the passes left.
 */
double ListDriver::restingOffset(double offset, const PassInput& input) const {
  if (input.tracking) {
    return offset;
  }
  double maxOffset = std::max(0.0, getContentAlong() - input.windowAlong);
  if (input.clampEnd && offset > maxOffset + input.clampSlack) {
    return maxOffset;
  }
  if (input.clampStart && offset < -input.clampSlack) {
    return 0.0;
  }
  return offset;
}

/*
 * Give the core the size of every row in its measured range that has none yet, then reflow once.
 * Returns whether any size changed. With prefetch on, the rows the host will mount get their
 * sizes first, which places them where they show, and the rows past them are prefetched before
 * they are measured.
 */
bool ListDriver::measureRange(double offset, const PassInput& input) {
  std::optional<MountedRange> measured = getMeasuredRange();
  if (!measured) {
    SizeBatch batch;
    return batch.commit(*core_);
  }
  auto outside = [&](std::size_t index) {
    return index < measured->low || index > measured->high ? index : UNDEFINED_INDEX;
  };
  // The pinned section header needs its real size even when it sits far above the measured range.
  std::size_t sticky = outside(activeStickyIndex(offset));
  /*
   * So does the row a command aims at. A saved position is a distance into its row, which can
   * reach past the end of an estimate.
   */
  std::size_t aimed = outside(rowAim_ ? indexOfKey(rowAim_->key) : UNDEFINED_INDEX);
  if (aimed == sticky) {
    aimed = UNDEFINED_INDEX;
  }
  if (!input.prefetch || !prefetchRows_) {
    return measureSpan(measured->low, measured->high, sticky, aimed);
  }
  bool changed = false;
  auto plan = [&] {
    return planMount(offset, input.windowAlong, input.mountPad, input.leadingInset, input.trailingInset);
  };
  MountPlan mounted = plan();
  // Sizes move the rows. A plan that grew reaches rows still placed by estimate.
  for (int round = 0; round < MAX_MOUNT_PLAN_ROUNDS && mounted.low != UNDEFINED_INDEX; ++round) {
    bool first = round == 0;
    if (!measureSpan(mounted.low, mounted.high, first ? sticky : UNDEFINED_INDEX, first ? aimed : UNDEFINED_INDEX)) {
      break;
    }
    changed = true;
    mounted = plan();
  }
  updatePrefetch(mounted.low, mounted.high, prefetchFound_, cancelFound_);
  if (!prefetchFound_.empty() || !cancelFound_.empty()) {
    prefetchRows_(prefetchFound_, cancelFound_);
  }
  return measureSpan(measured->low, measured->high, sticky, aimed) || changed;
}

/*
 * Give the core the size of every row from low to high, and of the pinned and the aimed row
 * outside them, that has none yet, then reflow once. Returns whether any size changed.
 */
bool ListDriver::measureSpan(std::size_t low, std::size_t high, std::size_t pinned, std::size_t aimed) {
  Container& core = *core_;
  SizeBatch batch;
  if (!measureRows_) {
    for (std::size_t index = low; index <= high; ++index) {
      measureIndex(index, batch);
    }
    for (std::size_t extra : {pinned, aimed}) {
      if (extra != UNDEFINED_INDEX) {
        measureIndex(extra, batch);
      }
    }
    return batch.commit(core);
  }
  measureIndices_.clear();
  measureCrosses_.clear();
  for (std::size_t index = low; index <= high; ++index) {
    collectIndex(index);
  }
  for (std::size_t extra : {pinned, aimed}) {
    if (extra != UNDEFINED_INDEX) {
      collectIndex(extra);
    }
  }
  if (!measureIndices_.empty()) {
    measureSizes_.assign(measureIndices_.size(), 0.0);
    measureRows_(measureIndices_, measureCrosses_, measureSizes_);
    bool horizontal = settings_.horizontal;
    for (std::size_t at = 0; at < measureIndices_.size(); ++at) {
      double along = measureSizes_[at];
      double cross = measureCrosses_[at];
      batch.apply(core, measureIndices_[at], horizontal ? Size{along, cross} : Size{cross, along});
    }
  }
  return batch.commit(core);
}

/*
 * Whether a row has no size yet or its content changed. A remeasured row counts once.
 */
bool ListDriver::needsMeasure(std::size_t index) {
  const Row& row = core_->revision.rows[index];
  bool remeasure = !remeasureKeys_.empty() && remeasureKeys_.erase(row.key) > 0;
  return !row.measured || remeasure;
}

double ListDriver::crossSizeOf(std::size_t index) const {
  const Row& row = core_->revision.rows[index];
  return settings_.numberOfColumns > 1 ? (settings_.horizontal ? row.height : row.width) : windowCross_;
}

void ListDriver::collectIndex(std::size_t index) {
  if (needsMeasure(index)) {
    measureIndices_.push_back(index);
    measureCrosses_.push_back(crossSizeOf(index));
  }
}

void ListDriver::measureIndex(std::size_t index, SizeBatch& batch) {
  if (!needsMeasure(index) || !measureRow_) {
    return;
  }
  double cross = crossSizeOf(index);
  double along = measureRow_(index, core_->revision.rows[index].key, cross);
  batch.apply(*core_, index, settings_.horizontal ? Size{along, cross} : Size{cross, along});
}

void ListDriver::resetKeepingPosition() {
  /*
   * The row at the viewport start keeps how far the viewport is into it. An inverted list
   * resting at its end stays at the end.
   */
  double offset = core_->getOffset();
  double windowAlong = core_->getWindowSize();
  bool holdEnd = settings_.inverted && getContentAlong() > windowAlong &&
    offset >= getContentAlong() - windowAlong - 1.0;
  std::optional<AnchorState> anchor = getAnchorState(offset);
  core_ = std::make_unique<Container>();
  installCallbacks();
  recordEdit({});
  nonAnchorKeysChanged_ = true;
  remeasureKeys_.clear();
  echoToken_ = 0;
  echoRevision_ = 0;
  dropHint_ = DropHint{};
  settlingLayouts_ = 0;
  rowAim_.reset();
  if (holdEnd) {
    core_->scrollToEnd();
  } else if (anchor) {
    restoreAnchorState(*anchor);
  }
}

std::size_t ListDriver::getRowCount() const {
  return core_->revision.rows.size();
}

std::uint64_t ListDriver::getGeometryVersion() const {
  return core_->state.geometryVersion;
}

double ListDriver::getContentAlong() const {
  return settings_.horizontal ? core_->revision.contentWidth : core_->revision.contentHeight;
}

bool ListDriver::isAtEnd() const {
  return !core_->state.invertedBottomReleased;
}

std::optional<MountedRange> ListDriver::getMeasuredRange() const {
  const Revision& revision = core_->revision;
  if (revision.rows.empty() || revision.measuredLow == UNDEFINED_INDEX || revision.measuredHigh == UNDEFINED_INDEX) {
    return std::nullopt;
  }
  return MountedRange{revision.measuredLow, std::min(revision.measuredHigh, revision.rows.size() - 1)};
}

std::optional<MountedRange> ListDriver::getVisibleRange() const {
  // The core's measured range holds the overscan too. These are the rows on screen.
  IndexRange visible = core_->getViewableIndices(ViewableRule{});
  if (visible.isEmpty()) {
    return std::nullopt;
  }
  return MountedRange{visible.low, visible.high};
}

RowRect ListDriver::getRowRect(std::size_t index) const {
  const Row& row = core_->revision.rows[index];
  if (settings_.numberOfColumns > 1) {
    return {row.offsetX, row.offsetY, row.width, row.height};
  }
  if (settings_.horizontal) {
    return {row.offsetX, 0.0, row.width, windowCross_};
  }
  return {0.0, row.offsetY, windowCross_, row.height};
}

double ListDriver::getLeadingAt(std::size_t index) const {
  const Row& row = core_->revision.rows[index];
  return settings_.horizontal ? row.offsetX : row.offsetY;
}

double ListDriver::getExtentAt(std::size_t index) const {
  const Row& row = core_->revision.rows[index];
  return settings_.horizontal ? row.width : row.height;
}

double ListDriver::getFooterStart(double footerSize) const {
  return core_->getFooterStart(footerSize);
}

std::size_t ListDriver::indexOfKey(const std::string& key) const {
  return core_->indexOfKey(key);
}

bool ListDriver::overlaps(std::size_t index, double viewLow, double viewHigh) const {
  double start = getLeadingAt(index);
  return start + getExtentAt(index) > viewLow && start < viewHigh;
}

MountPlan ListDriver::planMount(
  double offset,
  double windowAlong,
  double pad,
  double leadingInset,
  double trailingInset) const {
  MountPlan plan;
  plan.viewLow = offset - pad - leadingInset;
  plan.viewHigh = offset + windowAlong + pad + trailingInset;
  plan.sticky = activeStickyIndex(offset);
  plan.band = {offset, offset};
  std::optional<MountedRange> measured = getMeasuredRange();
  if (!measured) {
    return plan;
  }
  /*
   * A row overlaps the view at a new offset while start - after < offset < end + before. Each
   * end of the band stays a hair inside, where rounding cannot tell it from the edge.
   */
  constexpr double EDGE = 1e-6;
  double before = pad + leadingInset;
  double after = windowAlong + pad + trailingInset;
  double low = -std::numeric_limits<double>::infinity();
  double high = std::numeric_limits<double>::infinity();
  for (std::size_t index = measured->low; index <= measured->high; ++index) {
    double start = getLeadingAt(index);
    double end = start + getExtentAt(index);
    if (!overlaps(index, plan.viewLow, plan.viewHigh)) {
      if (end <= plan.viewLow) {
        low = std::max(low, end + before + EDGE);
      } else {
        high = std::min(high, start - after - EDGE);
      }
      continue;
    }
    low = std::max(low, start - after + EDGE);
    high = std::min(high, end + before - EDGE);
    if (plan.low == UNDEFINED_INDEX) {
      plan.low = index;
    }
    plan.high = index;
  }
  if (sticky_.empty()) {
    plan.band = {std::min(low, offset), std::max(high, offset)};
  }
  return plan;
}

bool ListDriver::shouldMount(const MountPlan& plan, std::size_t index) const {
  return settings_.numberOfColumns == 1 || overlaps(index, plan.viewLow, plan.viewHigh);
}

bool shouldSkipMount(
  const MountRecord& previous,
  const MountRecord& next,
  double offset,
  std::size_t numberOfColumns,
  bool holdsRow) {
  if (holdsRow || next.low != previous.low || next.high != previous.high || next.sticky != previous.sticky) {
    return false;
  }
  if (next.geometry != previous.geometry || next.structure != previous.structure || next.shift != previous.shift) {
    return false;
  }
  return numberOfColumns <= 1 || (previous.band.contains(offset) && next.inputs == previous.inputs);
}

/*
 * The shared search over the sorted sticky rows. Rows the core has not placed yet sort last.
 */
std::size_t ListDriver::activeStickyIndex(double offset) const {
  std::size_t rows = getRowCount();
  std::size_t position = pinnedSectionIndex(sticky_.size(), offset, [&](std::size_t at) {
    std::size_t index = sticky_[at];
    return index < rows ? getLeadingAt(index) : std::numeric_limits<double>::infinity();
  });
  return position == UNDEFINED_INDEX ? UNDEFINED_INDEX : sticky_[position];
}

double ListDriver::stickyLeading(std::size_t active, double offset) const {
  if (active >= getRowCount()) {
    return offset;
  }
  std::size_t index = active;
  auto next = std::upper_bound(sticky_.begin(), sticky_.end(), index);
  bool hasNext = next != sticky_.end() && *next < getRowCount();
  return pinnedSectionLeading(getLeadingAt(index), getExtentAt(index), offset, hasNext, hasNext ? getLeadingAt(*next) : 0.0);
}

void ListDriver::stickyFrames(std::vector<double>& out) const {
  out.clear();
  out.reserve(sticky_.size() * 2);
  for (std::size_t index : sticky_) {
    bool placed = index < getRowCount();
    out.push_back(placed ? getLeadingAt(index) : std::numeric_limits<double>::infinity());
    out.push_back(placed ? getExtentAt(index) : 0.0);
  }
}

void ListDriver::scrollToRow(std::size_t index, double viewPosition, double viewOffset) {
  aimAtRow(index, viewPosition, viewOffset);
}

/*
 * Send the core a scroll to a row and keep its aim for the passes that land it.
 */
void ListDriver::aimAtRow(std::size_t index, double viewPosition, double viewOffset) {
  rowAim_.reset();
  if (index < keys_.size()) {
    rowAim_ = RowAim{keys_[index], viewPosition, viewOffset};
  }
  core_->scrollToRow(index, viewPosition, viewOffset);
}

std::optional<AnchorState> ListDriver::getAnchorState(double offset) const {
  return anchorStateAt(*core_, offset);
}

bool ListDriver::restoreAnchorState(const AnchorState& anchor) {
  std::size_t index = UNDEFINED_INDEX;
  // Until the next edit the core's rows are the keys, and its key map finds the row.
  if (!keysChanged_ && !anchor.key.empty()) {
    index = core_->indexOfKey(anchor.key);
  } else {
    auto found = std::find(keys_.begin(), keys_.end(), anchor.key);
    if (found != keys_.end()) {
      index = static_cast<std::size_t>(found - keys_.begin());
    }
  }
  if (index == UNDEFINED_INDEX) {
    return false;
  }
  aimAtRow(index, 0.0, anchor.offset);
  return true;
}

void ListDriver::updatePrefetch(
  std::size_t mountedLow,
  std::size_t mountedHigh,
  std::vector<std::size_t>& prefetch,
  std::vector<std::size_t>& cancel) {
  std::optional<MountedRange> mounted;
  if (mountedLow != UNDEFINED_INDEX) {
    mounted = MountedRange{mountedLow, mountedHigh};
  }
  const std::vector<Row>& rows = core_->revision.rows;
  std::optional<MountedRange> range = getMeasuredRange();
  // The same rows and ranges as the previous call prefetch and cancel nothing.
  if (!prefetchStale_ && mounted == prefetchMounted_ && range == prefetchRange_ && rows.size() == prefetchCount_) {
    prefetch.clear();
    cancel.clear();
    return;
  }
  prefetchStale_ = false;
  prefetchMounted_ = mounted;
  prefetchRange_ = range;
  prefetchCount_ = rows.size();
  prefetch_.update(mounted, range, rows.size(),
    [&](std::size_t index) -> const std::string& { return rows[index].key; },
    [&](const std::string& key) { return indexOfKey(key); }, prefetch, cancel);
}

void ListDriver::scrollToStart() {
  rowAim_.reset();
  core_->scrollToStart();
}

void ListDriver::scrollToEnd() {
  rowAim_.reset();
  core_->scrollToEnd();
}

double ListDriver::nearestSnapOffset(double target) const {
  const std::vector<double>& offsets = core_->getSnapOffsets();
  if (offsets.empty()) {
    return target;
  }
  return azimgd::shadowlist::nearestSnapOffset(offsets, target);
}

double ListDriver::animatedTargetOffset(
  std::size_t index,
  double viewPosition,
  double windowAlong,
  double maxOffset) const {
  return rowTargetOffset(*core_, index, viewPosition, 0.0, windowAlong, maxOffset);
}

bool ListDriver::land() {
  ScrollLanding landing = landing_;
  landing_ = ScrollLanding{};
  switch (landing.target) {
    case ScrollLanding::Target::Row:
      scrollToRow(landing.index, landing.viewPosition);
      return true;
    case ScrollLanding::Target::Start:
      scrollToStart();
      return true;
    case ScrollLanding::Target::End:
      scrollToEnd();
      return true;
    case ScrollLanding::Target::None:
      return false;
  }
  return false;
}

DragRow ListDriver::dragRowAt(std::size_t index) const {
  const Row& placed = core_->revision.rows[index];
  bool horizontal = settings_.horizontal;
  DragRow row;
  row.index = index;
  row.leading = horizontal ? placed.offsetX : placed.offsetY;
  row.extent = horizontal ? placed.width : placed.height;
  if (settings_.numberOfColumns > 1) {
    row.crossLeading = horizontal ? placed.offsetY : placed.offsetX;
    row.crossExtent = horizontal ? placed.height : placed.width;
  }
  return row;
}

void ListDriver::dragBegin(std::size_t index, double touchAlong, double touchCross) {
  if (index >= getRowCount()) {
    return;
  }
  DragRow resting = dragRowAt(index);
  resting.key = core_->revision.rows[index].key;
  heldKey_ = resting.key;
  drag_.begin(resting, touchAlong, touchCross, settings_.numberOfColumns);
}

void ListDriver::dragEnd() {
  heldKey_.clear();
  drag_.end();
}

void ListDriver::setDropHint(DropHint hint) {
  std::lock_guard<std::recursive_mutex> lock(core_->coreMutex);
  dropHint_ = std::move(hint);
  core_->state.pendingDrop = DropHint{};
  core_->state.pendingDropAtMs = 0.0;
}

std::size_t ListDriver::getHeldIndex() const {
  return heldKey_.empty() ? UNDEFINED_INDEX : indexOfKey(heldKey_);
}

DragOffset ListDriver::placeHeld(std::size_t held, double touchAlong, double touchCross) {
  if (held >= getRowCount()) {
    return DragOffset{};
  }
  // The held row may have moved in the data since the drag began.
  drag_.updateOrigin(held, heldKey_);
  return drag_.placeRow(touchAlong, touchCross, dragRowAt(held), getContentAlong(), windowCross_);
}

void ListDriver::dragUpdateInsertion(const std::vector<std::size_t>& mounted) {
  std::size_t held = getHeldIndex();
  std::vector<DragRow>& rows = dragRows_;
  rows.clear();
  for (std::size_t index : mounted) {
    if (index < getRowCount() && index != held) {
      rows.push_back(dragRowAt(index));
    }
  }
  std::sort(rows.begin(), rows.end(), [](const DragRow& a, const DragRow& b) { return a.index < b.index; });
  const std::vector<Row>& placed = core_->revision.rows;
  drag_.updateInsertion(rows, [&placed](const DragRow& row) -> const std::string& { return placed[row.index].key; });
}

}
