#include <shadowlist-core/host/ListDriver.hpp>

#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/Snap.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace azimgd::shadowlist {

namespace {

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

void ListDriver::setSettings(const ListSettings& settings) {
  settings_ = settings;
  settings_.numberOfColumns = std::max<std::size_t>(1, settings.numberOfColumns);
}

void ListDriver::setStickyIndices(std::vector<std::size_t> sticky) {
  std::sort(sticky.begin(), sticky.end());
  sticky.erase(std::unique(sticky.begin(), sticky.end()), sticky.end());
  sticky_ = std::move(sticky);
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
  KeyEdit edit = endEdit(start, count, keys.size());
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

/*
 * A splice that only inserts or only deletes keys at an end, as an edit the core can trust.
 */
KeyEdit ListDriver::endEdit(std::size_t start, std::size_t deleted, std::size_t inserted) const {
  std::size_t size = keys_.size();
  if (size == 0 || (deleted > 0) == (inserted > 0) || deleted == size) {
    return {};
  }
  if (inserted > 0) {
    return {start == 0 ? KeyEditKind::Prepend : start == size ? KeyEditKind::Append : KeyEditKind::Unknown, inserted};
  }
  return {start == 0 ? KeyEditKind::TrimStart : start + deleted == size ? KeyEditKind::TrimEnd : KeyEditKind::Unknown,
    deleted};
}

void ListDriver::insertKeys(std::vector<std::size_t> indices, std::vector<std::string> keys) {
  // Pair each index with its key before sorting, then keep the first of any repeated index.
  std::vector<std::pair<std::size_t, std::string>> inserted;
  inserted.reserve(std::min(indices.size(), keys.size()));
  for (std::size_t position = 0; position < indices.size() && position < keys.size(); ++position) {
    inserted.emplace_back(indices[position], std::move(keys[position]));
  }
  std::stable_sort(inserted.begin(), inserted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  inserted.erase(std::unique(inserted.begin(), inserted.end(), [](const auto& a, const auto& b) { return a.first == b.first; }),
    inserted.end());
  if (inserted.empty()) {
    return;
  }
  std::size_t previousCount = keys_.size();
  std::size_t runs = countRuns(inserted.size(), [&](std::size_t at) { return inserted[at].first; });
  KeyEdit edit;
  if (runs == 1 && previousCount > 0) {
    std::size_t at = std::min(inserted.front().first, previousCount);
    edit.kind = at == 0 ? KeyEditKind::Prepend : at == previousCount ? KeyEditKind::Append : KeyEditKind::Unknown;
    edit.count = inserted.size();
  }
  if (runs <= MAX_IN_PLACE_KEY_RUNS) {
    insertKeysInPlace(inserted);
  } else {
    insertKeysRebuilding(inserted);
  }
  recordEdit(edit);
}

/*
 * Insert each run of adjacent new rows with one vector insert. Runs go in ascending order,
 * which leaves every row before the next run at its final index.
 */
void ListDriver::insertKeysInPlace(std::vector<std::pair<std::size_t, std::string>>& inserted) {
  std::size_t first = 0;
  while (first < inserted.size()) {
    std::size_t last = first + 1;
    while (last < inserted.size() && inserted[last].first == inserted[last - 1].first + 1) {
      ++last;
    }
    std::size_t at = std::min(inserted[first].first, keys_.size());
    std::vector<std::string> run;
    run.reserve(last - first);
    for (std::size_t position = first; position < last; ++position) {
      run.push_back(std::move(inserted[position].second));
    }
    keys_.insert(keys_.begin() + static_cast<std::ptrdiff_t>(at), std::make_move_iterator(run.begin()),
      std::make_move_iterator(run.end()));
    first = last;
  }
}

/*
 * Build the new key list in one pass, for inserts spread over many places.
 */
void ListDriver::insertKeysRebuilding(std::vector<std::pair<std::size_t, std::string>>& inserted) {
  std::vector<std::string> next;
  next.reserve(keys_.size() + inserted.size());
  std::size_t source = 0;
  for (auto& [index, key] : inserted) {
    // Keep the old keys that sit before this new row.
    while (next.size() < index && source < keys_.size()) {
      next.push_back(std::move(keys_[source++]));
    }
    next.push_back(std::move(key));
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
  if (runs == 1 && deleted.size() < keys_.size()) {
    edit.kind = deleted.front() == 0 ? KeyEditKind::TrimStart
      : deleted.back() + 1 == keys_.size() ? KeyEditKind::TrimEnd : KeyEditKind::Unknown;
    edit.count = deleted.size();
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
      remeasureKeys_.insert(keys_[index]);
    }
  }
}

FrameInput ListDriver::makeFrameInput(const PassInput& input, double offset, bool firstPass) const {
  FrameInput frame;
  frame.keysRef = &keys_;
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
  frame.headerSize = input.headerSize;
  frame.footerSize = input.footerSize;
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
  frame.userScrolled = firstPass && input.userScrolled;
  frame.commitToken = echoToken_;
  return frame;
}

PassResult ListDriver::runPasses(const PassInput& input) {
  Container& core = *core_;
  windowCross_ = input.windowCross;
  bool horizontal = settings_.horizontal;
  double offset = input.offset;
  for (int pass = 0; pass < MAX_CORE_PASSES_PER_LAYOUT; ++pass) {
    Virtualizer::update(core, makeFrameInput(input, offset, pass == 0));
    keysChanged_ = false;
    pendingEdit_ = KeyEdit{};
    editsSinceUpdate_ = 0;
    bool measured = measureWindow(offset);

    ContainerStateUpdate update = core.resolveStateUpdate(horizontal ? offset : 0, horizontal ? 0 : offset,
      core.revision.contentWidth, core.revision.contentHeight);
    bool wrote = false;
    if (update.applyOffset) {
      double target = clampOffset(horizontal ? update.offsetX : update.offsetY, input);
      if (update.commitToken != 0) {
        echoToken_ = update.commitToken;
      }
      if (std::fabs(target - offset) >= 0.01) {
        offset = target;
        wrote = true;
      }
    }
    if (!measured && !wrote && !core.hasPendingCommand()) {
      break;
    }
  }
  return finishPasses(offset);
}

/*
 * Pack the result and hand over the reached flags the passes raised.
 */
PassResult ListDriver::finishPasses(double offset) {
  const Container& core = *core_;
  PassResult result;
  result.offset = offset;
  result.contentAlong = getContentAlong();
  result.band = core.computeOffsetBand();
  settlingLayouts_ = core.hasPendingCommand() ? settlingLayouts_ + 1 : 0;
  result.settling = settlingLayouts_ > 0 && settlingLayouts_ < MAX_SETTLING_LAYOUTS;
  result.reachedStart = reachedStart_;
  result.reachedEnd = reachedEnd_;
  reachedStart_ = false;
  reachedEnd_ = false;
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
 * Give the core the size of every row in its window that has none yet, then reflow once.
 * Returns whether any size changed.
 */
bool ListDriver::measureWindow(double offset) {
  Container& core = *core_;
  SizeBatch batch;
  std::optional<MountedRange> window = getMeasuredRange();
  if (!window) {
    return batch.commit(core);
  }
  for (std::size_t index = window->low; index <= window->high; ++index) {
    measureIndex(index, batch);
  }
  // The pinned section header needs its real size even when it sits far above the window.
  std::size_t sticky = activeStickyIndex(offset);
  if (sticky != UNDEFINED_INDEX && (sticky < window->low || sticky > window->high)) {
    measureIndex(sticky, batch);
  }
  return batch.commit(core);
}

void ListDriver::measureIndex(std::size_t index, SizeBatch& batch) {
  Container& core = *core_;
  const Row& row = core.revision.rows[index];
  bool remeasure = !remeasureKeys_.empty() && remeasureKeys_.erase(row.key) > 0;
  if ((row.measured && !remeasure) || !measureRow_) {
    return;
  }
  bool horizontal = settings_.horizontal;
  double cross = settings_.numberOfColumns > 1 ? (horizontal ? row.height : row.width) : windowCross_;
  double along = measureRow_(index, row.key, cross);
  batch.apply(core, index, horizontal ? Size{along, cross} : Size{cross, along});
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
  remeasureKeys_.clear();
  echoToken_ = 0;
  settlingLayouts_ = 0;
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
  return core_->geometryVersion;
}

double ListDriver::getContentAlong() const {
  return settings_.horizontal ? core_->revision.contentWidth : core_->revision.contentHeight;
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
  std::optional<MountedRange> window = getMeasuredRange();
  if (!window) {
    return plan;
  }
  for (std::size_t index = window->low; index <= window->high; ++index) {
    if (!overlaps(index, plan.viewLow, plan.viewHigh)) {
      continue;
    }
    if (plan.low == UNDEFINED_INDEX) {
      plan.low = index;
    }
    plan.high = index;
  }
  return plan;
}

bool ListDriver::shouldMount(const MountPlan& plan, std::size_t index) const {
  return settings_.numberOfColumns == 1 || overlaps(index, plan.viewLow, plan.viewHigh);
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
  core_->scrollToRow(index, viewPosition, viewOffset);
}

std::optional<AnchorState> ListDriver::getAnchorState(double offset) const {
  return anchorStateAt(*core_, offset);
}

bool ListDriver::restoreAnchorState(const AnchorState& anchor) {
  auto found = std::find(keys_.begin(), keys_.end(), anchor.key);
  if (found == keys_.end()) {
    return false;
  }
  core_->scrollToRow(static_cast<std::size_t>(found - keys_.begin()), 0.0, anchor.offset);
  return true;
}

void ListDriver::updatePrefetch(
  std::size_t mountedLow,
  std::size_t mountedHigh,
  std::vector<std::size_t>& prefetch,
  std::vector<std::size_t>& cancel) {
  prefetch.clear();
  cancel.clear();
  std::optional<MountedRange> window = getMeasuredRange();
  auto mounted = [&](std::size_t index) {
    return mountedLow != UNDEFINED_INDEX && index >= mountedLow && index <= mountedHigh;
  };
  auto inWindow = [&](std::size_t index) { return window && index >= window->low && index <= window->high; };
  // Rows prefetched earlier: shown now, gone, or out of the window.
  for (auto entry = prefetched_.begin(); entry != prefetched_.end();) {
    std::size_t index = indexOfKey(*entry);
    if (index == UNDEFINED_INDEX || mounted(index)) {
      entry = prefetched_.erase(entry);
    } else if (!inWindow(index)) {
      cancel.push_back(index);
      entry = prefetched_.erase(entry);
    } else {
      ++entry;
    }
  }
  if (window) {
    const std::vector<Row>& rows = core_->revision.rows;
    for (std::size_t index = window->low; index <= window->high && index < rows.size(); ++index) {
      if (!mounted(index) && prefetched_.insert(rows[index].key).second) {
        prefetch.push_back(index);
      }
    }
  }
  std::sort(cancel.begin(), cancel.end());
}

void ListDriver::scrollToStart() {
  core_->scrollToStart();
}

void ListDriver::scrollToEnd() {
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
      core_->scrollToRow(landing.index, landing.viewPosition);
      return true;
    case ScrollLanding::Target::Start:
      core_->scrollToStart();
      return true;
    case ScrollLanding::Target::End:
      core_->scrollToEnd();
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
  row.key = placed.key;
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
  heldKey_ = resting.key;
  drag_.begin(resting, touchAlong, touchCross, settings_.numberOfColumns);
}

void ListDriver::dragEnd() {
  heldKey_.clear();
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
  std::vector<DragRow> rows;
  rows.reserve(mounted.size());
  for (std::size_t index : mounted) {
    if (index < getRowCount() && index != held) {
      rows.push_back(dragRowAt(index));
    }
  }
  std::sort(rows.begin(), rows.end(), [](const DragRow& a, const DragRow& b) { return a.index < b.index; });
  drag_.updateInsertion(rows);
}

}
