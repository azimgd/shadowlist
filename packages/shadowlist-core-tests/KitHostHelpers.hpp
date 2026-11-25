#pragma once

#include "FuzzHelpers.hpp"
#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/MountedRange.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>
#include <shadowlist-core/host/StickyLayout.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/*
 * KitHost, a small model of a native list view around one ListDriver, with the checks every
 * frame it lays out has to pass.
 */
namespace slt {

using namespace azimgd::shadowlist;

inline constexpr double SECTION_HEADER_SIZE = 36.0;
inline constexpr double SECTION_FOOTER_SIZE = 24.0;

/*
 * A cell the kit made, and the row and frame its last mount pass gave it.
 */
struct KitCell {
  std::size_t id = 0;
  std::size_t row = UNDEFINED_INDEX;
  std::uint64_t generation = 0;
  RowRect frame;
};

/*
 * One section of the data source.
 */
struct KitSection {
  std::string key;
  std::size_t itemCount = 0;
  bool hasHeader = false;
  bool hasFooter = false;
};

/*
 * A stand in for ShadowListKitListView: the data source, the scroll view and the layout pass
 * around one ListDriver. It reaches the core only through the host layer.
 */
struct KitHost : FuzzHost {
  ListDriver driver;
  ListSections sections;
  BatchCollector batch;
  ContentVersions contentVersions;

  /*
   * The data source: item keys, sections when it has any, and the content version of each
   * row whose content changed under its key. The others are at version 0.
   */
  std::vector<std::string> items;
  std::vector<KitSection> sectionModel;
  std::unordered_map<std::string, std::int64_t> versions;

  /*
   * The list's properties and the bounds its superview gives it.
   */
  ListSettings settings;
  double boundsAlong = WINDOW_HEIGHT;
  double boundsCross = WINDOW_WIDTH;
  double headerView = 0.0;
  double footerView = 0.0;
  double emptyView = 0.0;
  bool stickyHeader = false;
  bool stickyFooter = false;
  double mountOverscan = 0.5;
  bool stickySectionHeaders = false;
  std::vector<std::size_t> stickyRows;
  std::unordered_set<std::string> nonAnchorKeys;
  bool prefetches = false;
  bool inWindow = false;

  /*
   * Whether the list moves its own offset, like the Android kit, or sits in a scroll view that
   * owns the bounce and rests on device pixels, like the Apple kits.
   */
  bool movesOwnOffset = false;

  /*
   * What the scroll view holds and reports.
   */
  double offset = 0.0;
  double contentAlong = 0.0;
  ScrollPhase phase = ScrollPhase::Idle;
  bool tracking = false;

  /*
   * The kit's own state between layout passes.
   */
  ListSettings sentSettings;
  bool hasSentSettings = false;
  double windowAlong = 0.0;
  double windowCross = 0.0;
  double headerSize = 0.0;
  double footerSize = 0.0;
  double emptySize = 0.0;
  double footerStart = 0.0;
  bool showsEmpty = true;
  double previousOffset = 0.0;
  bool userScrolled = false;
  bool needsFrame = true;
  bool settleScheduled = false;
  bool inLayout = false;
  OffsetBand band;
  bool pendingReachedStart = false;
  bool pendingReachedEnd = false;
  bool atEnd = true;
  bool pendingAtEnd = false;
  std::uint64_t structureVersion = 0;

  /*
   * Where the last layout pass placed the header, the footer and the empty view along the axis.
   */
  double headerStart = 0.0;
  double footerShown = 0.0;
  double emptyStart = 0.0;

  /*
   * The mounted cells by row key, the reuse pool and what the last mount pass covered.
   */
  std::map<std::string, KitCell> mounted;
  std::vector<std::size_t> pool;
  MountPlan plan;
  MountRecord mountRecord;
  bool mountedThisFrame = false;
  std::uint64_t mountGeneration = 0;
  std::vector<std::size_t> prefetchRows;
  std::vector<std::size_t> cancelRows;

  /*
   * What the delegate and the data source saw.
   */
  int reachedStart = 0;
  int reachedEnd = 0;
  int atEndChanges = 0;
  std::size_t reachedStartCount = UNDEFINED_INDEX;
  std::size_t reachedEndCount = UNDEFINED_INDEX;
  std::size_t cellsCreated = 0;
  std::size_t cellsDisplayed = 0;
  std::size_t cellsEnded = 0;
  std::unordered_set<std::string> measuredKeys;
  std::unordered_set<std::string> prefetchedKeys;

  /*
   * Whether a gesture took the reader of an inverted list off its newest row. Only a real move
   * back onto it, by the reader or a scroll command, ends that. Content that shrinks onto the
   * reader does not.
   */
  bool leftBottom = false;

  /*
   * The first thing a callback saw go wrong. The layout pass that ran it fails with it.
   */
  std::string fault;

  /*
   * The geometry version the mounted cells were last walked for.
   */
  std::uint64_t walkedGeometry = 0;

  /*
   * The structure version rest last compared the keys and looked every key up for.
   */
  std::uint64_t checkedStructure = UNDEFINED_INDEX;

  KitHost(std::uint32_t seed, FuzzEpisode& episode)
    : FuzzHost(seed, episode) {
    // The Apple kits measure a row at a time. The Android and web kits measure a pass's rows at once.
    movesOwnOffset = !chance(rng, 50);
    if (!movesOwnOffset) {
      driver.setMeasureRow([this](std::size_t index, const std::string& key, double cross) {
        if (index >= driver.getKeyCount() || driver.getKeyAt(index) != key) {
          raise("measure asked for row " + std::to_string(index) + " under a key the list does not hold there");
          return 1.0;
        }
        return measure(index, cross);
      });
    } else {
      driver.setMeasureRows(
        [this](
          const std::vector<std::size_t>& indices, const std::vector<double>& crossSizes, std::vector<double>& sizes) {
          for (std::size_t at = 0; at < indices.size(); ++at) {
            sizes[at] = measure(indices[at], crossSizes[at]);
          }
        });
    }
    prefetches = chance(rng, 30);
    driver.setPrefetchRows([this](const std::vector<std::size_t>& prefetch, const std::vector<std::size_t>& cancel) {
      reportPrefetch(prefetch, cancel);
    });
  }

  void raise(const std::string& what) {
    if (fault.empty()) {
      fault = what;
    }
  }

  /*
   * A row's size along the scroll axis for a cross size. A narrower row is longer, like
   * wrapped text.
   */
  double sizeFor(const std::string& key, double cross) const {
    double base = SECTION_HEADER_SIZE;
    auto found = trueSizes.find(key);
    if (found != trueSizes.end()) {
      base = found->second;
    } else if (key.rfind(ListSections::FOOTER_KEY_PREFIX, 0) == 0) {
      base = SECTION_FOOTER_SIZE;
    }
    return base * (0.5 + 0.5 * WINDOW_WIDTH / std::max(cross, 1.0));
  }

  /*
   * The data source's answer to a measure call. A row is measured once until it is reloaded or
   * every size is dropped.
   */
  double measure(std::size_t index, double cross) {
    if (index >= driver.getKeyCount()) {
      raise("measure asked for row " + std::to_string(index) + " past the keys");
      return 1.0;
    }
    const std::string& key = driver.getKeyAt(index);
    if (!measuredKeys.insert(key).second) {
      raise("row " + key + " was measured again without a reload");
    }
    return sizeFor(key, cross);
  }

  /*
   * Prefetch and cancel lists name rows low to high, and a cancel only follows a prefetch.
   */
  void reportPrefetch(const std::vector<std::size_t>& prefetch, const std::vector<std::size_t>& cancel) {
    auto ordered = [&](const std::vector<std::size_t>& rows, const char* name) {
      for (std::size_t at = 0; at < rows.size(); ++at) {
        if (rows[at] >= driver.getKeyCount() || (at > 0 && rows[at] <= rows[at - 1])) {
          raise(std::string(name) + " rows are not low to high inside the keys");
          return false;
        }
      }
      return true;
    };
    if (!ordered(prefetch, "prefetch") || !ordered(cancel, "cancel")) {
      return;
    }
    for (std::size_t row : cancel) {
      if (prefetchedKeys.erase(driver.getKeyAt(row)) == 0) {
        raise("row " + driver.getKeyAt(row) + " was cancelled without a prefetch");
      }
    }
    for (std::size_t row : prefetch) {
      prefetchedKeys.insert(driver.getKeyAt(row));
    }
  }

  double maxOffset() const { return std::max(0.0, contentAlong - windowAlong); }

  std::size_t getColumns() const { return std::max<std::size_t>(1, settings.numberOfColumns); }

  /*
   * The nonAnchorKeys property: the driver takes the keys and the next layout pass runs the core.
   */
  void setNonAnchorKeys(std::unordered_set<std::string> keys) {
    nonAnchorKeys = std::move(keys);
    std::vector<std::string> sorted(nonAnchorKeys.begin(), nonAnchorKeys.end());
    std::sort(sorted.begin(), sorted.end());
    driver.setNonAnchorKeys(std::move(sorted));
    needsFrame = true;
  }

  /*
   * Where the rows start: after the header, and after the empty view while it shows.
   */
  double rowsStart() const { return headerSize + (driver.showsEmpty() ? emptySize : 0.0); }

  /*
   * Read the sections and every key the way reloadData does. Returns the rows' keys.
   */
  std::vector<std::string> readRowKeys() {
    if (sectionModel.empty()) {
      sections.setPlain(items.size());
      return items;
    }
    std::vector<SectionSpec> specs;
    std::vector<std::optional<std::string>> sectionKeys;
    for (const KitSection& section : sectionModel) {
      specs.push_back({section.itemCount, section.hasHeader, section.hasFooter});
      sectionKeys.push_back(section.key);
    }
    sections.setSections(std::move(specs));
    return sections.rowKeys(items, sectionKeys);
  }

  std::vector<std::int64_t> versionsOf(const std::vector<std::string>& keys) const {
    std::vector<std::int64_t> read;
    read.reserve(keys.size());
    for (const std::string& key : keys) {
      auto found = versions.find(key);
      read.push_back(found == versions.end() ? 0 : found->second);
    }
    return read;
  }

  /*
   * The driver's reloadKeys with the splice worked out against the keys it holds.
   */
  void reloadDriverKeys(std::vector<std::string> keys) {
    KeySplice splice = keySplice(driver.getKeys(), keys);
    driver.reloadKeys(std::move(keys), splice);
  }

  void structureChanged() {
    ++structureVersion;
    if (stickySectionHeaders) {
      stickyRows = sections.stickyRows({}, true);
      driver.setStickyIndices(stickyRows);
    }
    needsFrame = true;
  }

  /*
   * A data change lays out right away while the list is in a window.
   */
  void layoutDataChange() {
    if (!inWindow || inLayout || batch.isCollecting()) {
      return;
    }
    layoutPass();
  }

  void reloadData() {
    std::vector<std::string> keys = readRowKeys();
    contentVersions.record(items, versionsOf(items));
    reloadDriverKeys(std::move(keys));
    structureChanged();
    layoutDataChange();
  }

  /*
   * indices are positions in the data source's items now, which already hold the new rows.
   */
  void insertItems(const std::vector<std::size_t>& indices) {
    if (!sectionModel.empty()) {
      reloadData();
      return;
    }
    std::vector<std::size_t> inserted = insertionIndices(indices, driver.getKeyCount());
    std::vector<std::string> keys;
    for (std::size_t index : inserted) {
      keys.push_back(items[index]);
    }
    insertContentVersions(inserted);
    driver.insertKeys(std::move(inserted), std::move(keys));
    sections.setPlain(driver.getKeyCount());
    structureChanged();
    layoutDataChange();
  }

  /*
   * The content version of items a change inserted, which the next applyChanges compares against.
   */
  void insertContentVersions(const std::vector<std::size_t>& inserted) {
    std::vector<std::int64_t> read;
    read.reserve(inserted.size());
    for (std::size_t item : inserted) {
      auto found = versions.find(items[item]);
      read.push_back(found == versions.end() ? 0 : found->second);
    }
    contentVersions.insert(items, inserted, read);
  }

  /*
   * indices are positions in the items from before the change.
   */
  void deleteItems(const std::vector<std::size_t>& indices) {
    if (!sectionModel.empty()) {
      reloadData();
      return;
    }
    std::vector<std::size_t> deleted = deletionIndices(indices, driver.getKeyCount());
    if (deleted.empty()) {
      return;
    }
    driver.deleteKeys(std::move(deleted));
    sections.setPlain(driver.getKeyCount());
    structureChanged();
    layoutDataChange();
  }

  void reloadItems(const std::vector<std::size_t>& reloaded) {
    reloadRows(rowsOfItems(reloaded));
    layoutDataChange();
  }

  std::vector<std::size_t> rowsOfItems(const std::vector<std::size_t>& reloaded) const {
    std::vector<std::size_t> rows;
    for (std::size_t item : reloaded) {
      std::size_t row = sections.rowForItem(item);
      if (row != UNDEFINED_INDEX) {
        rows.push_back(row);
      }
    }
    return rows;
  }

  /*
   * Recycle the rows' cells and measure the rows again.
   */
  void reloadRows(const std::vector<std::size_t>& rows) {
    for (std::size_t row : rows) {
      if (row >= driver.getKeyCount()) {
        continue;
      }
      const std::string& key = driver.getKeyAt(row);
      measuredKeys.erase(key);
      auto cell = mounted.find(key);
      if (cell != mounted.end()) {
        recycle(cell->second);
        mounted.erase(cell);
      }
    }
    driver.markRemeasure(rows);
    structureChanged();
  }

  /*
   * performBatchUpdates with the changes its block made. The data source already holds the
   * items after the batch.
   */
  void performBatchUpdates(const BatchUpdate& update) {
    batch.begin();
    batch.addDeleted(update.deleted);
    batch.addInserted(update.inserted);
    for (const KeyMove& move : update.moved) {
      batch.addMoved(move.from, move.to);
    }
    batch.addReloaded(update.reloaded);
    if (!batch.end()) {
      return;
    }
    CollectedBatch collected = batch.take();
    if (collected.isEmpty()) {
      return;
    }
    std::unordered_set<std::string> reloadedKeys = keysOfRows(driver.getKeys(), rowsOfItems(collected.update.reloaded));
    std::vector<std::string> next;
    std::vector<std::size_t> inserted;
    bool planned = false;
    if (sectionModel.empty() && !collected.needsReload) {
      std::optional<BatchPlan> batchPlan = planBatch(driver.getKeyCount(), items.size(), collected.update);
      if (batchPlan) {
        next = keysFromPlan(*batchPlan, driver.getKeys(), [this](std::size_t index) { return items[index]; });
        inserted = insertedRows(*batchPlan, driver.getKeyCount());
        sections.setPlain(next.size());
        planned = true;
      } else {
        raise("a batch that adds up was not planned");
      }
    }
    if (!planned) {
      std::vector<std::string> scratch;
      std::vector<std::string> previousItems = sections.itemKeys(driver.getKeys(), scratch);
      next = readRowKeys();
      inserted = diffKeys(previousItems, items).inserted;
    }
    insertContentVersions(inserted);
    commitKeyChanges(std::move(next), reloadedKeys);
  }

  void commitKeyChanges(std::vector<std::string> next, const std::unordered_set<std::string>& reloadedKeys) {
    reloadDriverKeys(std::move(next));
    std::vector<std::size_t> rows = rowsOfKeys(driver.getKeys(), reloadedKeys);
    if (rows.empty()) {
      structureChanged();
    } else {
      reloadRows(rows);
    }
    layoutDataChange();
  }

  /*
   * applyChanges: the items are read again, diffed against the ones held, and the ones whose
   * content version changed are reloaded.
   */
  void applyChanges() {
    std::vector<std::string> scratch;
    std::vector<std::string> previousItems = sections.itemKeys(driver.getKeys(), scratch);
    std::vector<std::string> next = readRowKeys();
    checkDiff(previousItems, items, diffKeys(previousItems, items));
    std::unordered_set<std::string> reloadedKeys;
    for (std::size_t item : contentVersions.update(items, versionsOf(items))) {
      reloadedKeys.insert(items[item]);
    }
    commitKeyChanges(std::move(next), reloadedKeys);
  }

  /*
   * A diff names every key that left and every key that came, each once and in order, and a
   * moved key is the same key at both of its places.
   */
  void checkDiff(const std::vector<std::string>& previous, const std::vector<std::string>& next, const KeyDiff& diff) {
    // Item keys are numbered, and a flag per number tells where a key is.
    std::vector<char> isBefore(nextKey, 0);
    std::vector<char> isAfter(nextKey, 0);
    for (const std::string& key : previous) {
      isBefore[numberOf(key)] = 1;
    }
    for (const std::string& key : next) {
      isAfter[numberOf(key)] = 1;
    }
    struct Keys {
      const std::vector<char>& flags;
      std::size_t count(const std::string& key) const { return flags[numberOf(key)]; }
    };
    Keys before{isBefore};
    Keys after{isAfter};
    auto ascending = [](const std::vector<std::size_t>& indices) {
      return std::adjacent_find(indices.begin(), indices.end(), std::greater_equal<std::size_t>()) == indices.end();
    };
    if (!ascending(diff.deleted) || !ascending(diff.inserted)) {
      failWith("diff: indices are not ascending");
    }
    std::size_t gone = 0;
    for (const std::string& key : previous) {
      gone += after.count(key) == 0 ? 1 : 0;
    }
    std::size_t come = 0;
    for (const std::string& key : next) {
      come += before.count(key) == 0 ? 1 : 0;
    }
    if (diff.deleted.size() != gone || diff.inserted.size() != come) {
      failWith(
        "diff: " + std::to_string(diff.deleted.size()) + " deleted and " + std::to_string(diff.inserted.size()) +
        " inserted for " + std::to_string(gone) + " keys gone and " + std::to_string(come) + " new");
    }
    for (std::size_t index : diff.deleted) {
      if (index >= previous.size() || after.count(previous[index]) > 0) {
        failWith("diff: deleted index " + std::to_string(index) + " names a key that stayed");
      }
    }
    for (std::size_t index : diff.inserted) {
      if (index >= next.size() || before.count(next[index]) > 0) {
        failWith("diff: inserted index " + std::to_string(index) + " names a key that was there");
      }
    }
    for (const KeyMove& move : diff.moved) {
      if (move.from >= previous.size() || move.to >= next.size() || previous[move.from] != next[move.to]) {
        failWith("diff: move " + std::to_string(move.from) + " to " + std::to_string(move.to) + " changes the key");
      }
    }
  }

  /*
   * Read the bounds and template sizes. A new cross size drops every size and keeps the first
   * visible row. Returns whether any changed.
   */
  bool readGeometry() {
    if (boundsCross != windowCross && windowCross > 0.0) {
      driver.resetKeepingPosition();
      measuredKeys.clear();
      band = OffsetBand();
      needsFrame = true;
      ++structureVersion;
    }
    bool changed = boundsAlong != windowAlong || boundsCross != windowCross || headerView != headerSize ||
                   footerView != footerSize || emptyView != emptySize;
    windowAlong = boundsAlong;
    windowCross = boundsCross;
    headerSize = headerView;
    footerSize = footerView;
    emptySize = emptyView;
    return changed;
  }

  void trackUserScroll() {
    if (std::fabs(offset - previousOffset) < OFFSET_MOVED_THRESHOLD) {
      return;
    }
    if (phase != ScrollPhase::Idle) {
      userScrolled = true;
    }
    previousOffset = offset;
  }

  void writeOffset(double value) {
    previousOffset = value;
    offset = value;
  }

  void runPasses() {
    if (!hasSentSettings || settings != sentSettings) {
      driver.setSettings(settings);
      sentSettings = settings;
      hasSentSettings = true;
    }
    PassInput input;
    input.offset = offset;
    input.windowAlong = windowAlong;
    input.windowCross = windowCross;
    input.headerSize = headerSize;
    input.footerSize = footerSize;
    input.emptySize = emptySize;
    input.phase = phase;
    input.userScrolled = userScrolled;
    input.tracking = tracking;
    input.prefetch = prefetches;
    input.mountPad = windowAlong * mountOverscan;
    // A list that moves its own offset comes back from either end whenever no finger holds it.
    input.clampStart = movesOwnOffset;
    input.clampEnd = movesOwnOffset || phase == ScrollPhase::Idle;
    input.clampSlack = movesOwnOffset ? 0.0 : 0.5;
    PassResult result = driver.runPasses(input);
    userScrolled = false;
    needsFrame = false;
    if (std::fabs(result.contentAlong - driver.getContentAlong()) > 0.01) {
      raise("the pass result's content size is not the driver's");
    }
    applyPassResult(result);
  }

  /*
   * The content size goes first, then the offset. The driver hands an edge it reached over once.
   */
  void applyPassResult(const PassResult& result) {
    contentAlong = result.contentAlong;
    pendingReachedStart = pendingReachedStart || result.reachedStart;
    pendingReachedEnd = pendingReachedEnd || result.reachedEnd;
    if (result.atEnd != atEnd) {
      atEnd = result.atEnd;
      pendingAtEnd = !pendingAtEnd;
    }
    showsEmpty = result.showsEmpty;
    if (std::fabs(result.offset - offset) >= 0.01) {
      writeOffset(result.offset);
    }
    band = result.band;
    footerStart = result.footerStart;
    if (result.settling) {
      needsFrame = true;
      settleScheduled = true;
    }
  }

  /*
   * Place the header, the footer and the empty view. A pinned header or footer moves by the host
   * layer's translation, on every pass.
   */
  void layoutTemplates() {
    StickyInput input;
    input.offset = offset;
    input.windowSize = windowAlong;
    input.contentSize = contentAlong;
    input.hasHeader = headerSize > 0.0;
    input.headerSize = headerSize;
    input.stickyHeader = stickyHeader;
    input.hasFooter = footerSize > 0.0;
    input.footerSize = footerSize;
    input.footerStart = footerStart;
    input.stickyFooter = stickyFooter;
    StickyState state;
    StickyTranslations translations = stickyTranslations(input, state);
    headerStart = translations.header;
    footerShown = footerStart + translations.footer;
    emptyStart = headerSize;
  }

  void recycle(const KitCell& cell) {
    ++cellsEnded;
    pool.push_back(cell.id);
  }

  void unmountAll() {
    for (const auto& entry : mounted) {
      recycle(entry.second);
    }
    mounted.clear();
    plan = MountPlan();
    mountRecord = MountRecord();
  }

  void mountRow(std::size_t index) {
    if (index >= driver.getRowCount() || index >= driver.getKeyCount()) {
      return;
    }
    const std::string& key = driver.getKeyAt(index);
    auto found = mounted.find(key);
    if (found == mounted.end()) {
      KitCell cell;
      if (pool.empty()) {
        cell.id = cellsCreated++;
      } else {
        cell.id = pool.back();
        pool.pop_back();
      }
      ++cellsDisplayed;
      found = mounted.emplace(key, cell).first;
    }
    found->second.row = index;
    found->second.generation = mountGeneration;
    found->second.frame = driver.getRowRect(index);
  }

  /*
   * Mount the cells of the rows near the viewport and recycle the rest. Rows are matched by key.
   * A pass that covers what the last one did is skipped, see shouldSkipMount.
   */
  void mountCells() {
    if (!driver.getMeasuredRange()) {
      unmountAll();
      return;
    }
    MountInputs inputs;
    inputs.windowAlong = windowAlong;
    inputs.pad = windowAlong * mountOverscan;
    plan = driver.planMount(offset, inputs.windowAlong, inputs.pad);
    MountRecord next{plan.low,         plan.high, plan.sticky, driver.getGeometryVersion(),
                     structureVersion, 0.0,       plan.band,   inputs};
    bool skipped = shouldSkipMount(mountRecord, next, offset, getColumns(), false);
    mountRecord = next;
    if (skipped) {
      return;
    }
    mountedThisFrame = true;
    ++mountGeneration;
    for (std::size_t index = plan.low; plan.low != UNDEFINED_INDEX && index <= plan.high; ++index) {
      if (driver.shouldMount(plan, index)) {
        mountRow(index);
      }
    }
    if (plan.sticky != UNDEFINED_INDEX) {
      mountRow(plan.sticky);
    }
    for (auto entry = mounted.begin(); entry != mounted.end();) {
      if (entry->second.generation != mountGeneration) {
        recycle(entry->second);
        entry = mounted.erase(entry);
      } else {
        ++entry;
      }
    }
    if (prefetches) {
      driver.updatePrefetch(plan.low, plan.high, prefetchRows, cancelRows);
      reportPrefetch(prefetchRows, cancelRows);
    }
  }

  void dispatchReached() {
    if (pendingReachedStart) {
      pendingReachedStart = false;
      ++reachedStart;
      reachedStartCount = driver.getKeyCount();
    }
    if (pendingReachedEnd) {
      pendingReachedEnd = false;
      ++reachedEnd;
      reachedEndCount = driver.getKeyCount();
    }
    if (pendingAtEnd) {
      pendingAtEnd = false;
      ++atEndChanges;
    }
  }

  /*
   * One layout pass: the core runs when a frame was asked for, the geometry changed or the
   * offset left the band, then the cells mount. Everything lands before the frame is drawn, and
   * the frame is checked as drawn.
   */
  void layoutPass() {
    inLayout = true;
    mountedThisFrame = false;
    bool geometryChanged = readGeometry();
    trackUserScroll();
    bool ran = false;
    if (windowCross > 0.0 && windowAlong > 0.0) {
      if (needsFrame || geometryChanged || !band.contains(offset)) {
        ran = true;
        runPasses();
      }
      layoutTemplates();
      mountCells();
    }
    inLayout = false;
    dispatchReached();
    observeFrame(ran);
    verify();
  }

  /*
   * Lay out again while a correction waits for the next display frame. Returns the frames it
   * took, or -1 when the driver kept asking.
   */
  int settle() {
    for (int frame = 0; frame < FUZZ_MAX_SETTLE_FRAMES; ++frame) {
      if (!settleScheduled) {
        return frame;
      }
      settleScheduled = false;
      layoutPass();
    }
    return settleScheduled ? -1 : FUZZ_MAX_SETTLE_FRAMES;
  }

  /*
   * Fold what the frame shows into the episode's digest.
   */
  void observeFrame(bool ran) {
    std::optional<MountedRange> measured = driver.getMeasuredRange();
    episode.observe(offset);
    episode.observe(contentAlong);
    episode.observe(driver.getRowCount());
    episode.observe(measured ? measured->low : UNDEFINED_INDEX);
    episode.observe(measured ? measured->high : UNDEFINED_INDEX);
    episode.observe(plan.low);
    episode.observe(plan.high);
    episode.observe(plan.sticky);
    episode.observe(mounted.size());
    episode.observe(driver.getGeometryVersion());
    episode.observe(settleScheduled);
    episode.observe(reachedStart);
    episode.observe(reachedEnd);
    episode.observe(atEnd);
    episode.observe(headerStart);
    episode.observe(footerShown);
    if (tracing) {
      std::printf(
        "      %s off=%.1f total=%.1f max=%.1f n=%zu win=[%zd..%zd] plan=[%zd..%zd] sticky=%zd cells=%zu "
        "band=[%.1f..%.1f] settling=%d geometry=%llu reached=%d/%d\n",
        ran ? "core" : "band", offset, contentAlong, maxOffset(), driver.getRowCount(),
        static_cast<std::ptrdiff_t>(measured ? measured->low : UNDEFINED_INDEX),
        static_cast<std::ptrdiff_t>(measured ? measured->high : UNDEFINED_INDEX), static_cast<std::ptrdiff_t>(plan.low),
        static_cast<std::ptrdiff_t>(plan.high), static_cast<std::ptrdiff_t>(plan.sticky), mounted.size(), band.low,
        band.high, settleScheduled ? 1 : 0, static_cast<unsigned long long>(driver.getGeometryVersion()), reachedStart,
        reachedEnd);
    }
  }

  /*
   * The first row that ends after position. One column is in order and searched. With more
   * columns every row is a candidate.
   */
  std::size_t firstRowEndingAfter(double position) const {
    if (getColumns() > 1) {
      return 0;
    }
    std::size_t low = 0;
    std::size_t high = driver.getRowCount();
    while (low < high) {
      std::size_t middle = low + (high - low) / 2;
      if (driver.getLeadingAt(middle) + driver.getExtentAt(middle) <= position) {
        low = middle + 1;
      } else {
        high = middle;
      }
    }
    return low;
  }

  /*
   * Calls visit with every row that overlaps the viewport, low to high, until it returns false.
   */
  template <typename Visit>
  void forEachRowOnScreen(const Visit& visit) const {
    std::size_t count = driver.getRowCount();
    for (std::size_t index = firstRowEndingAfter(offset); index < count; ++index) {
      double start = driver.getLeadingAt(index);
      if (start >= offset + windowAlong) {
        if (getColumns() == 1) {
          return;
        }
        continue;
      }
      if (start + driver.getExtentAt(index) > offset && !visit(index, start)) {
        return;
      }
    }
  }

  /*
   * The rows the driver placed are the keys it holds, contiguous per track from the header, and
   * the content ends with the footer after the longest track. A frame walks the measured rows
   * and their neighbors, where a pass moves things. With all set every row is walked.
   */
  std::string checkGeometry(bool all) const {
    std::size_t count = driver.getRowCount();
    if (count != driver.getKeyCount()) {
      return std::to_string(count) + " rows placed for " + std::to_string(driver.getKeyCount()) + " keys";
    }
    std::size_t tracks = getColumns();
    std::optional<MountedRange> measured = driver.getMeasuredRange();
    std::size_t low = all || !measured ? 0 : measured->low - std::min(measured->low, tracks);
    std::size_t high = all || !measured ? count : std::min(count, measured->high + tracks + 1);
    for (std::size_t index = low; index < high; ++index) {
      double start = driver.getLeadingAt(index);
      double size = driver.getExtentAt(index);
      // A row follows the one before it on its track, and the first of a track the header.
      double edge =
        index < tracks ? rowsStart() : driver.getLeadingAt(index - tracks) + driver.getExtentAt(index - tracks);
      if (!std::isfinite(start) || !std::isfinite(size) || size < 0.0) {
        return "non finite geometry at " + std::to_string(index);
      }
      if (std::fabs(start - edge) > 0.01) {
        return "row " + std::to_string(index) + " at " + std::to_string(start) + " expected " + std::to_string(edge);
      }
      RowRect rect = driver.getRowRect(index);
      double rectStart = settings.horizontal ? rect.x : rect.y;
      double rectSize = settings.horizontal ? rect.width : rect.height;
      double rectCross = settings.horizontal ? rect.height : rect.width;
      if (rectStart != start || rectSize != size) {
        return "row " + std::to_string(index) + " has a frame off its place along the axis";
      }
      if (tracks == 1 && rectCross != windowCross) {
        return "row " + std::to_string(index) + " does not span the cross axis";
      }
    }
    // Every track ends with one of the last rows.
    double end = rowsStart();
    for (std::size_t index = count - std::min(count, tracks); index < count; ++index) {
      end = std::max(end, driver.getLeadingAt(index) + driver.getExtentAt(index));
    }
    double total = end + footerSize;
    if (std::fabs(driver.getContentAlong() - total) > 0.01) {
      return "total " + std::to_string(driver.getContentAlong()) + " expected " + std::to_string(total);
    }
    if (std::fabs(driver.getFooterStart(footerSize) + footerSize - total) > 0.01) {
      return "footer at " + std::to_string(driver.getFooterStart(footerSize)) + " does not end the content of " +
             std::to_string(total);
    }
    return "";
  }

  /*
   * Every mounted cell shows the row its key names, at the frame the driver reports now and at
   * the row's real size. Every row on screen is mounted. Each cell is on screen or in the pool.
   */
  std::string checkMount() {
    std::size_t count = driver.getRowCount();
    // Cells a skipped mount pass left alone are walked again once the driver says a row moved.
    bool walk = mountedThisFrame || driver.getGeometryVersion() != walkedGeometry;
    walkedGeometry = driver.getGeometryVersion();
    for (const auto& [key, cell] : mounted) {
      if (!walk) {
        break;
      }
      if (cell.row >= count || driver.getKeyAt(cell.row) != key) {
        return "cell of " + key + " sits on row " + std::to_string(cell.row) + " that has another key";
      }
      RowRect rect = driver.getRowRect(cell.row);
      if (
        rect.x != cell.frame.x || rect.y != cell.frame.y || rect.width != cell.frame.width ||
        rect.height != cell.frame.height) {
        return "cell of " + key + " kept a frame the row " + std::to_string(cell.row) + " moved away from";
      }
      double cross = getColumns() > 1 ? (settings.horizontal ? rect.height : rect.width) : windowCross;
      double size = sizeFor(key, cross);
      if (std::fabs(driver.getExtentAt(cell.row) - size) > 0.01) {
        return "row " + std::to_string(cell.row) + " is mounted at " + std::to_string(driver.getExtentAt(cell.row)) +
               " and measures " + std::to_string(size);
      }
    }
    std::size_t bare = UNDEFINED_INDEX;
    forEachRowOnScreen([&](std::size_t index, double) {
      auto found = mounted.find(driver.getKeyAt(index));
      if (found == mounted.end() || found->second.row != index) {
        bare = index;
      }
      return bare == UNDEFINED_INDEX;
    });
    if (bare != UNDEFINED_INDEX) {
      return "row " + std::to_string(bare) + " is on screen without a cell, plan [" +
             std::to_string(static_cast<std::ptrdiff_t>(plan.low)) + ".." +
             std::to_string(static_cast<std::ptrdiff_t>(plan.high)) + "] offset=" + std::to_string(offset);
    }
    if (cellsCreated != mounted.size() + pool.size() || cellsDisplayed - cellsEnded != mounted.size()) {
      return "cells do not add up: " + std::to_string(cellsCreated) + " made, " + std::to_string(mounted.size()) +
             " mounted, " + std::to_string(pool.size()) + " pooled, " + std::to_string(cellsDisplayed) + " shown, " +
             std::to_string(cellsEnded) + " ended";
    }
    return "";
  }

  /*
   * The mount plan holds its rows over its whole band, and the pinned header is the last sticky
   * row at or above the offset, kept between its own place and the next header.
   */
  std::string checkPlan() const {
    if (!driver.getMeasuredRange()) {
      return mounted.empty() ? "" : "cells are mounted without a measured range";
    }
    if (!plan.band.contains(offset)) {
      return "the mount plan's band [" + std::to_string(plan.band.low) + ".." + std::to_string(plan.band.high) +
             "] leaves out its own offset " + std::to_string(offset);
    }
    for (double edge : {plan.band.low, plan.band.high}) {
      // A frame that kept its cells was checked when they were mounted.
      if (!std::isfinite(edge) || !mountedThisFrame) {
        continue;
      }
      MountPlan other = driver.planMount(edge, windowAlong, windowAlong * mountOverscan);
      bool bothMount = plan.low != UNDEFINED_INDEX && other.low != UNDEFINED_INDEX;
      std::size_t highest = std::max(plan.high, other.high);
      for (std::size_t index = std::min(plan.low, other.low); bothMount && index <= highest; ++index) {
        bool here = index >= plan.low && index <= plan.high && driver.shouldMount(plan, index);
        bool there = index >= other.low && index <= other.high && driver.shouldMount(other, index);
        if (here != there) {
          return "the mount plan's band reaches " + std::to_string(edge) + " where row " + std::to_string(index) +
                 " mounts differently than at " + std::to_string(offset);
        }
      }
      if ((plan.low == UNDEFINED_INDEX) != (other.low == UNDEFINED_INDEX)) {
        return "the mount plan's band reaches " + std::to_string(edge) + " where the plan is empty or not";
      }
    }
    if (!driver.hasSticky()) {
      return plan.sticky == UNDEFINED_INDEX ? "" : "a pinned row without sticky rows";
    }
    std::size_t active = UNDEFINED_INDEX;
    std::size_t following = UNDEFINED_INDEX;
    for (std::size_t row : stickyRows) {
      if (row >= driver.getRowCount()) {
        break;
      }
      if (driver.getLeadingAt(row) <= offset) {
        active = row;
      } else {
        following = row;
        break;
      }
    }
    if (plan.sticky != active || driver.activeStickyIndex(offset) != active) {
      return "pinned row " + std::to_string(static_cast<std::ptrdiff_t>(plan.sticky)) + " where " +
             std::to_string(static_cast<std::ptrdiff_t>(active)) + " is the last header above " +
             std::to_string(offset);
    }
    if (active == UNDEFINED_INDEX) {
      return "";
    }
    if (mounted.count(driver.getKeyAt(active)) == 0) {
      return "the pinned row " + std::to_string(active) + " has no cell";
    }
    double pinned = driver.stickyLeading(active, offset);
    double limit = following == UNDEFINED_INDEX
                     ? offset
                     : std::min(offset, driver.getLeadingAt(following) - driver.getExtentAt(active));
    if (std::fabs(pinned - limit) > 0.01) {
      return "the pinned row sits at " + std::to_string(pinned) + " expected " + std::to_string(limit);
    }
    return "";
  }

  /*
   * The empty view shows while the list has no rows, between the header and the footer. A
   * pinned header sits at the viewport start until the footer's place pushes it off, and never
   * before its own place. A pinned footer ends at the viewport end and never rides into the
   * header's place. Neither moves without its switch.
   */
  std::string checkTemplates() const {
    if (showsEmpty != (driver.getKeyCount() == 0)) {
      return "the empty view shows for " + std::to_string(driver.getKeyCount()) + " rows";
    }
    if (showsEmpty && std::fabs(footerStart - (headerSize + emptySize)) > 0.01) {
      return "the footer of an empty list starts at " + std::to_string(footerStart) + " and the empty view ends at " +
             std::to_string(headerSize + emptySize);
    }
    if (headerSize > 0.0) {
      double pushed = contentAlong - footerSize - headerSize;
      double expected = stickyHeader ? std::min(offset, pushed) : 0.0;
      if (std::fabs(headerStart - expected) > 0.01 || (offset >= 0.0 && headerStart < -0.01)) {
        return "the header sits at " + std::to_string(headerStart) + " expected " + std::to_string(expected);
      }
    }
    if (footerSize > 0.0) {
      double expected = stickyFooter ? std::max(offset + windowAlong - footerSize, headerSize) : footerStart;
      if (std::fabs(footerShown - expected) > 0.01) {
        return "the footer sits at " + std::to_string(footerShown) + " expected " + std::to_string(expected);
      }
    }
    return "";
  }

  void verify() {
    if (!fault.empty()) {
      failWith(fault);
    }
    if (windowAlong <= 0.0 || windowCross <= 0.0) {
      return;
    }
    std::string templates = checkTemplates();
    if (!templates.empty()) {
      failWith("templates: " + templates);
    }
    std::string geometry = checkGeometry(false);
    if (!geometry.empty()) {
      failWith("geometry: " + geometry);
    }
    std::string mount = checkMount();
    if (!mount.empty()) {
      failWith("mount: " + mount);
    }
    std::string planned = checkPlan();
    if (!planned.empty()) {
      failWith("plan: " + planned);
    }
  }
};

}
