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

void ListDriver::setMeasureItem(MeasureItem measureItem) {
  measureItem_ = std::move(measureItem);
}

void ListDriver::setSettings(const ListSettings& settings) {
  settings_ = settings;
  settings_.columns = std::max<std::size_t>(1, settings.columns);
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
  std::size_t limit = std::min(keys_.size(), keys.size());
  std::size_t start = 0;
  while (start < limit && keys_[start] == keys[start]) {
    ++start;
  }
  std::size_t end = 0;
  while (end < limit - start && keys_[keys_.size() - 1 - end] == keys[keys.size() - 1 - end]) {
    ++end;
  }
  if (start == keys_.size() && start == keys.size()) {
    return;
  }
  std::vector<std::string> middle(std::make_move_iterator(keys.begin() + static_cast<std::ptrdiff_t>(start)),
    std::make_move_iterator(keys.end() - static_cast<std::ptrdiff_t>(end)));
  replaceKeys(start, keys_.size() - start - end, std::move(middle));
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
 * A splice that only adds or only removes keys at an end, as an edit the core can trust.
 */
KeyEdit ListDriver::endEdit(std::size_t start, std::size_t removed, std::size_t added) const {
  std::size_t size = keys_.size();
  if (size == 0 || (removed > 0) == (added > 0) || removed == size) {
    return {};
  }
  if (added > 0) {
    return {start == 0 ? KeyEditKind::Prepend : start == size ? KeyEditKind::Append : KeyEditKind::Unknown, added};
  }
  return {start == 0 ? KeyEditKind::TrimStart : start + removed == size ? KeyEditKind::TrimEnd : KeyEditKind::Unknown,
    removed};
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
  std::vector<std::size_t> removed = deletionPositions(std::move(indices), keys_.size());
  if (removed.empty()) {
    return;
  }
  std::size_t runs = countRuns(removed.size(), [&](std::size_t at) { return removed[at]; });
  KeyEdit edit;
  if (runs == 1 && removed.size() < keys_.size()) {
    edit.kind = removed.front() == 0 ? KeyEditKind::TrimStart
      : removed.back() + 1 == keys_.size() ? KeyEditKind::TrimEnd : KeyEditKind::Unknown;
    edit.count = removed.size();
  }
  if (runs <= MAX_IN_PLACE_KEY_RUNS) {
    // Erase runs from the last one back, which keeps the earlier indices valid.
    std::size_t last = removed.size();
    while (last > 0) {
      std::size_t first = last - 1;
      while (first > 0 && removed[first - 1] + 1 == removed[first]) {
        --first;
      }
      keys_.erase(keys_.begin() + static_cast<std::ptrdiff_t>(removed[first]),
        keys_.begin() + static_cast<std::ptrdiff_t>(removed[last - 1] + 1));
      last = first;
    }
  } else {
    std::vector<std::string> next;
    next.reserve(keys_.size() - removed.size());
    std::size_t position = 0;
    for (std::size_t index = 0; index < keys_.size(); ++index) {
      if (position < removed.size() && removed[position] == index) {
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
  double cellCross = input.windowCross / static_cast<double>(settings_.columns);
  if (settings_.horizontal) {
    frame.containerOffsetX = offset;
    frame.windowContainerWidth = input.windowAlong;
    frame.windowContainerHeight = input.windowCross;
    frame.estimatedElementSize = {settings_.estimatedItemSize, cellCross};
  } else {
    frame.containerOffsetY = offset;
    frame.windowContainerWidth = input.windowCross;
    frame.windowContainerHeight = input.windowAlong;
    frame.estimatedElementSize = {cellCross, settings_.estimatedItemSize};
  }
  frame.headerSize = input.headerSize;
  frame.footerSize = input.footerSize;
  frame.inverted = settings_.inverted;
  frame.followAppends = settings_.followAppends;
  frame.horizontal = settings_.horizontal;
  frame.columns = settings_.columns;
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
      core.revision.totalContainerWidth, core.revision.totalContainerHeight);
    bool wrote = false;
    if (update.applyContainerOffset) {
      double target = clampOffset(horizontal ? update.containerOffsetX : update.containerOffsetY, input);
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
  std::optional<MountedRange> window = getMeasuredWindow();
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
  const Element& element = core.revision.elements[index];
  bool remeasure = !remeasureKeys_.empty() && remeasureKeys_.erase(element.key) > 0;
  if ((element.measured && !remeasure) || !measureItem_) {
    return;
  }
  bool horizontal = settings_.horizontal;
  double cross = settings_.columns > 1 ? (horizontal ? element.height : element.width) : windowCross_;
  double along = measureItem_(index, element.key, cross);
  batch.apply(core, index, horizontal ? Size{along, cross} : Size{cross, along});
}

void ListDriver::resetKeepingPosition() {
  std::optional<MountedRange> visible = getVisibleRange();
  core_ = std::make_unique<Container>();
  installCallbacks();
  recordEdit({});
  remeasureKeys_.clear();
  echoToken_ = 0;
  settlingLayouts_ = 0;
  if (visible && visible->low < keys_.size()) {
    core_->scrollToIndex(visible->low, 0);
  }
}

std::size_t ListDriver::getCount() const {
  return core_->revision.elements.size();
}

std::uint64_t ListDriver::getGeometryVersion() const {
  return core_->geometryVersion;
}

double ListDriver::getContentAlong() const {
  return settings_.horizontal ? core_->revision.totalContainerWidth : core_->revision.totalContainerHeight;
}

std::optional<MountedRange> ListDriver::getMeasuredWindow() const {
  const Revision& revision = core_->revision;
  std::size_t first = revision.measurementElementStartIndex;
  std::size_t last = revision.measurementElementEndIndex;
  if (revision.elements.empty() || first == UNDEFINED_INDEX || last == UNDEFINED_INDEX) {
    return std::nullopt;
  }
  std::size_t low = std::min(first, last);
  std::size_t high = std::min(std::max(first, last), revision.elements.size() - 1);
  return MountedRange{low, high};
}

std::optional<MountedRange> ListDriver::getVisibleRange() const {
  auto visible = core_->getVisibleIndices();
  if (visible.first == UNDEFINED_INDEX || visible.second == UNDEFINED_INDEX) {
    return std::nullopt;
  }
  return MountedRange{std::min(visible.first, visible.second), std::max(visible.first, visible.second)};
}

RowRect ListDriver::getRowRect(std::size_t index) const {
  const Element& element = core_->revision.elements[index];
  if (settings_.columns > 1) {
    return {element.offsetX, element.offsetY, element.width, element.height};
  }
  if (settings_.horizontal) {
    return {element.offsetX, 0.0, element.width, windowCross_};
  }
  return {0.0, element.offsetY, windowCross_, element.height};
}

double ListDriver::getLeadingAt(std::size_t index) const {
  const Element& element = core_->revision.elements[index];
  return settings_.horizontal ? element.offsetX : element.offsetY;
}

double ListDriver::getExtentAt(std::size_t index) const {
  const Element& element = core_->revision.elements[index];
  return settings_.horizontal ? element.width : element.height;
}

double ListDriver::getFooterStart(double footerSize) const {
  return core_->getFooterOffset(footerSize);
}

std::size_t ListDriver::indexOfKey(const std::string& key) const {
  return core_->findElementIndexByKey(key);
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
  std::optional<MountedRange> window = getMeasuredWindow();
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
  return settings_.columns == 1 || overlaps(index, plan.viewLow, plan.viewHigh);
}

/*
 * The shared search over the sorted sticky rows. Rows the core has not placed yet sort last.
 */
std::size_t ListDriver::activeStickyIndex(double offset) const {
  std::size_t rows = getCount();
  std::size_t position = pinnedSectionPosition(sticky_.size(), offset, [&](std::size_t at) {
    std::size_t index = sticky_[at];
    return index < rows ? getLeadingAt(index) : std::numeric_limits<double>::infinity();
  });
  return position == UNDEFINED_INDEX ? UNDEFINED_INDEX : sticky_[position];
}

double ListDriver::stickyLeading(std::size_t active, double offset) const {
  if (active >= getCount()) {
    return offset;
  }
  std::size_t index = active;
  auto next = std::upper_bound(sticky_.begin(), sticky_.end(), index);
  bool hasNext = next != sticky_.end() && *next < getCount();
  return pinnedSectionLeading(getLeadingAt(index), getExtentAt(index), offset, hasNext, hasNext ? getLeadingAt(*next) : 0.0);
}

void ListDriver::stickyFrames(std::vector<double>& out) const {
  out.clear();
  out.reserve(sticky_.size() * 2);
  for (std::size_t index : sticky_) {
    bool known = index < getCount();
    out.push_back(known ? getLeadingAt(index) : 0.0);
    out.push_back(known ? getExtentAt(index) : 0.0);
  }
}

void ListDriver::scrollToIndex(std::size_t index, double viewPosition, double rowOffset) {
  core_->scrollToIndex(index, viewPosition, rowOffset);
}

std::optional<ListAnchor> ListDriver::getAnchor(double offset) const {
  return anchorAt(*core_, offset);
}

bool ListDriver::restoreAnchor(const ListAnchor& anchor) {
  auto found = std::find(keys_.begin(), keys_.end(), anchor.key);
  if (found == keys_.end()) {
    return false;
  }
  core_->scrollToIndex(static_cast<std::size_t>(found - keys_.begin()), 0.0, anchor.offset);
  return true;
}

void ListDriver::updatePrefetch(
  std::size_t mountedLow,
  std::size_t mountedHigh,
  std::vector<std::size_t>& prefetch,
  std::vector<std::size_t>& cancel) {
  prefetch.clear();
  cancel.clear();
  std::optional<MountedRange> window = getMeasuredWindow();
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
    const std::vector<Element>& elements = core_->revision.elements;
    for (std::size_t index = window->low; index <= window->high && index < elements.size(); ++index) {
      if (!mounted(index) && prefetched_.insert(elements[index].key).second) {
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
    case ScrollLanding::Target::Index:
      core_->scrollToIndex(landing.index, landing.viewPosition);
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
  const Element& element = core_->revision.elements[index];
  bool horizontal = settings_.horizontal;
  DragRow row;
  row.index = index;
  row.key = element.key;
  row.leading = horizontal ? element.offsetX : element.offsetY;
  row.extent = horizontal ? element.width : element.height;
  if (settings_.columns > 1) {
    row.crossLeading = horizontal ? element.offsetY : element.offsetX;
    row.crossExtent = horizontal ? element.height : element.width;
  }
  return row;
}

void ListDriver::dragBegin(std::size_t index, double touchAlong, double touchCross) {
  if (index >= getCount()) {
    return;
  }
  DragRow resting = dragRowAt(index);
  heldKey_ = resting.key;
  drag_.begin(resting, touchAlong, touchCross, settings_.columns);
}

void ListDriver::dragEnd() {
  heldKey_.clear();
}

std::size_t ListDriver::getHeldIndex() const {
  return heldKey_.empty() ? UNDEFINED_INDEX : indexOfKey(heldKey_);
}

DragOffset ListDriver::placeHeld(std::size_t held, double touchAlong, double touchCross) {
  if (held >= getCount()) {
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
    if (index < getCount() && index != held) {
      rows.push_back(dragRowAt(index));
    }
  }
  std::sort(rows.begin(), rows.end(), [](const DragRow& a, const DragRow& b) { return a.index < b.index; });
  drag_.updateInsertion(rows);
}

}
