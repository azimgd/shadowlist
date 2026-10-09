#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitChangeAnimator.h"
#import "Internal/ShadowListKitListModels+Private.h"

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>

using namespace azimgd::shadowlist;

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * Data changes. Every change hands the core the next keys, marks the structure changed and lays
 * the change out right away while the list is on screen.
 */
@implementation ShadowListKitListView (Data)

- (BOOL)hasSections
{
  return [_dataSource respondsToSelector:@selector(numberOfSectionsInListView:)];
}

/*
 * Read the sections and every key. Sets the sections and returns the rows' keys, which include
 * section headers and footers.
 */
- (std::vector<std::string>)readRowKeys
{
  _sizesFromDataSource = [_dataSource respondsToSelector:@selector(listView:sizeForItemAtIndex:crossSize:)];
  if (![self hasSections]) {
    NSInteger count = MAX(0, [_dataSource numberOfItemsInListView:self]);
    std::vector<std::string> keys;
    keys.reserve(count);
    for (NSInteger index = 0; index < count; ++index) {
      keys.push_back(ShadowListKitStdString([_dataSource listView:self keyForItemAtIndex:index]));
    }
    _sections.setPlain((std::size_t)count);
    return keys;
  }
  NSInteger sectionCount = MAX(0, [_dataSource numberOfSectionsInListView:self]);
  BOOL headers = [_dataSource respondsToSelector:@selector(listView:titleForHeaderInSection:)];
  BOOL footers = [_dataSource respondsToSelector:@selector(listView:titleForFooterInSection:)];
  BOOL sectionKeys = [_dataSource respondsToSelector:@selector(listView:keyForSection:)];
  std::vector<SectionSpec> specs;
  std::vector<std::string> itemKeys;
  std::vector<std::optional<std::string>> keysOfSections(sectionCount);
  specs.reserve(sectionCount);
  for (NSInteger section = 0; section < sectionCount; ++section) {
    SectionSpec spec;
    spec.itemCount = (std::size_t)MAX(0, [_dataSource listView:self numberOfItemsInSection:section]);
    spec.hasHeader = headers && [_dataSource listView:self titleForHeaderInSection:section] != nil;
    spec.hasFooter = footers && [_dataSource listView:self titleForFooterInSection:section] != nil;
    std::size_t first = itemKeys.size();
    for (std::size_t local = 0; local < spec.itemCount; ++local) {
      itemKeys.push_back(ShadowListKitStdString([_dataSource listView:self keyForItemAtIndex:(NSInteger)(first + local)]));
    }
    if (sectionKeys) {
      keysOfSections[section] = ShadowListKitStdString([_dataSource listView:self keyForSection:section]);
    }
    specs.push_back(spec);
  }
  _sections.setSections(std::move(specs));
  return _sections.rowKeys(itemKeys, keysOfSections);
}

- (void)reloadData
{
  if (_batchDepth > 0) {
    _batchNeedsReload = YES;
    return;
  }
  std::vector<std::string> keys = [self readRowKeys];
  [self recordContentVersions:keys];
  [self applyRowKeys:std::move(keys)];
  [self reloadSectionIndex];
  [self layoutDataChange];
}

/*
 * The content version of every item now, which the next applyChanges compares against.
 */
- (void)recordContentVersions:(const std::vector<std::string>&)rowKeys
{
  if (![_dataSource respondsToSelector:@selector(listView:contentVersionForItemAtIndex:)]) {
    return;
  }
  std::vector<std::string> items = _sections.itemKeys(rowKeys);
  _contentVersions.record(items, [self contentVersionsOfItems:items.size()]);
}

- (std::vector<std::int64_t>)contentVersionsOfItems:(std::size_t)count
{
  std::vector<std::int64_t> versions(count);
  for (std::size_t item = 0; item < count; ++item) {
    versions[item] = [_dataSource listView:self contentVersionForItemAtIndex:(NSInteger)item];
  }
  return versions;
}

/*
 * Hand the rows' keys to the core. Only the keys between the unchanged rows at both ends
 * reach it.
 */
- (void)applyRowKeys:(std::vector<std::string>)keys
{
  if (_animatesChanges) {
    [self captureChangeTo:keys];
  }
  _driver.reloadKeys(std::move(keys));
  [self structureChanged];
}

- (void)insertItemsAtIndices:(NSIndexSet *)indices
{
  if (indices.count == 0) {
    return;
  }
  if (_batchDepth > 0) {
    std::vector<std::size_t> inserted = ShadowListKitIndices(indices);
    _batch.inserted.insert(_batch.inserted.end(), inserted.begin(), inserted.end());
    return;
  }
  if ([self hasSections]) {
    [self reloadData];
    return;
  }
  // An index past the end inserts at the end. The key is read where the row lands.
  std::vector<std::size_t> inserted = insertionPositions(ShadowListKitIndices(indices), _driver.getKeyCount());
  std::vector<std::string> keys;
  keys.reserve(inserted.size());
  for (std::size_t index : inserted) {
    keys.push_back(ShadowListKitStdString([_dataSource listView:self keyForItemAtIndex:(NSInteger)index]));
  }
  if (_animatesChanges) {
    [_changes captureRemoved:{} inserted:keys];
  }
  _driver.insertKeys(std::move(inserted), std::move(keys));
  _sections.setPlain(_driver.getKeyCount());
  [self structureChanged];
  [self layoutDataChange];
}

- (void)deleteItemsAtIndices:(NSIndexSet *)indices
{
  if (indices.count == 0) {
    return;
  }
  if (_batchDepth > 0) {
    std::vector<std::size_t> deleted = ShadowListKitIndices(indices);
    _batch.deleted.insert(_batch.deleted.end(), deleted.begin(), deleted.end());
    return;
  }
  if ([self hasSections]) {
    [self reloadData];
    return;
  }
  std::vector<std::size_t> deleted = deletionPositions(ShadowListKitIndices(indices), _driver.getKeyCount());
  if (deleted.empty()) {
    return;
  }
  if (_animatesChanges) {
    std::vector<std::string> removed;
    removed.reserve(deleted.size());
    for (std::size_t index : deleted) {
      removed.push_back(_driver.getKeyAt(index));
    }
    [_changes captureRemoved:removed inserted:{}];
  }
  _driver.deleteKeys(std::move(deleted));
  _sections.setPlain(_driver.getKeyCount());
  [self structureChanged];
  [self layoutDataChange];
}

- (void)reloadItemsAtIndices:(NSIndexSet *)indices
{
  [self reloadItemsAtIndices:indices payload:nil];
}

- (void)reloadItemsAtIndices:(NSIndexSet *)indices payload:(id)payload
{
  if (_batchDepth > 0) {
    std::vector<std::size_t> reloaded = ShadowListKitIndices(indices);
    _batch.reloaded.insert(_batch.reloaded.end(), reloaded.begin(), reloaded.end());
    _batchPayload = payload;
    return;
  }
  std::vector<std::size_t> rows;
  for (std::size_t item : ShadowListKitIndices(indices)) {
    std::size_t row = _sections.rowForItem(item);
    if (row != UNDEFINED_INDEX) {
      rows.push_back(row);
    }
  }
  [self reloadRows:rows payload:payload];
  [self layoutDataChange];
}

/*
 * Configure the rows' cells again and measure them again. With a payload, the data source may
 * update a shown cell in place.
 */
- (void)reloadRows:(const std::vector<std::size_t>&)rows payload:(id)payload
{
  BOOL reconfigures = payload != nil &&
    [_dataSource respondsToSelector:@selector(listView:reconfigureCell:atIndex:payload:)];
  for (std::size_t row : rows) {
    ShadowListKitListCell *cell = [self mountedCellAtIndex:row];
    if (!cell) {
      continue;
    }
    std::size_t item = _sections.itemForRow(row);
    if (reconfigures && item != UNDEFINED_INDEX &&
        [_dataSource listView:self reconfigureCell:cell atIndex:(NSInteger)item payload:payload]) {
      continue;
    }
    _mounted.erase(_driver.getKeyAt(row));
    [self recycleCell:cell];
  }
  _driver.markRemeasure(rows);
  [self structureChanged];
}

- (void)moveItemAtIndex:(NSInteger)index toIndex:(NSInteger)newIndex
{
  if (index < 0 || newIndex < 0) {
    return;
  }
  [self performBatchUpdates:^{
    self->_batch.moved.push_back({(std::size_t)index, (std::size_t)newIndex});
  } completion:nil];
}

- (void)performBatchUpdates:(void (NS_NOESCAPE ^)(void))updates completion:(void (^)(BOOL))completion
{
  ++_batchDepth;
  if (updates) {
    updates();
  }
  --_batchDepth;
  if (_batchDepth == 0) {
    [self commitBatch];
  }
  if (!completion) {
    return;
  }
  // The change animation runs after the next layout. Completion follows it.
  NSTimeInterval wait = 0;
  if (_animatesChanges && self.window && [_itemAnimator isKindOfClass:[ShadowListKitDefaultItemAnimator class]]) {
    wait = ((ShadowListKitDefaultItemAnimator *)_itemAnimator).duration;
  }
  if (self.window) {
    [self layoutIfNeeded];
  }
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(wait * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
    completion(YES);
  });
}

/*
 * Apply the changes a batch collected in one go. The batch plan builds the next keys from the
 * keys held and the new ones it reads. A list with sections, or a batch that does not add up,
 * reads everything again.
 */
- (void)commitBatch
{
  BatchUpdate batch = std::move(_batch);
  _batch = BatchUpdate{};
  id payload = _batchPayload;
  _batchPayload = nil;
  BOOL needsReload = _batchNeedsReload;
  _batchNeedsReload = NO;
  if (batch.isEmpty() && !needsReload) {
    return;
  }
  // The reloaded rows by key, from the data before.
  std::unordered_set<std::string> reloadedKeys = keysOfRows(_driver.getKeys(), [self rowsOfItems:batch.reloaded]);
  std::vector<std::string> next;
  BOOL planned = NO;
  if (![self hasSections] && !needsReload) {
    NSInteger nextCount = MAX(0, [_dataSource numberOfItemsInListView:self]);
    std::optional<BatchPlan> plan = planBatch(_driver.getKeyCount(), (std::size_t)nextCount, batch);
    if (plan) {
      next = keysFromPlan(*plan, _driver.getKeys(), [self](std::size_t index) {
        return ShadowListKitStdString([_dataSource listView:self keyForItemAtIndex:(NSInteger)index]);
      });
      _sections.setPlain(next.size());
      planned = YES;
    } else {
      NSLog(@"[ShadowListKitListView] batch updates do not add up to %ld items, reloading", (long)nextCount);
    }
  }
  if (!planned) {
    next = [self readRowKeys];
  }
  [self commitKeyChanges:std::move(next) reloadedKeys:reloadedKeys payload:payload reloadsSectionIndex:!planned];
}

/*
 * Hand the next keys to the core, reload the rows of reloadedKeys that remain and lay the change
 * out. A list whose sections may have changed reads its section index again.
 */
- (void)commitKeyChanges:(std::vector<std::string>)next
            reloadedKeys:(const std::unordered_set<std::string>&)reloadedKeys
                 payload:(id)payload
     reloadsSectionIndex:(BOOL)reloadsSectionIndex
{
  if (_animatesChanges) {
    [self captureChangeTo:next];
  }
  _driver.reloadKeys(std::move(next));
  std::vector<std::size_t> rows = rowsOfKeys(_driver.getKeys(), reloadedKeys);
  if (rows.empty()) {
    [self structureChanged];
  } else {
    [self reloadRows:rows payload:payload];
  }
  if (reloadsSectionIndex) {
    [self reloadSectionIndex];
  }
  [self layoutDataChange];
}

/*
 * The rows of items, skipping items past the end.
 */
- (std::vector<std::size_t>)rowsOfItems:(const std::vector<std::size_t>&)items
{
  std::vector<std::size_t> rows;
  rows.reserve(items.size());
  for (std::size_t item : items) {
    std::size_t row = _sections.rowForItem(item);
    if (row != UNDEFINED_INDEX) {
      rows.push_back(row);
    }
  }
  return rows;
}

- (ShadowListKitListChanges *)applyChanges
{
  std::vector<std::string> previousItems = _sections.itemKeys(_driver.getKeys());
  std::vector<std::string> next = [self readRowKeys];
  std::vector<std::string> nextItems = _sections.itemKeys(next);
  KeyDiff diff = diffKeys(previousItems, nextItems);

  // Rows that stayed but whose content version changed get reloaded.
  NSMutableIndexSet *reloaded = [NSMutableIndexSet indexSet];
  std::unordered_set<std::string> reloadedKeys;
  if ([_dataSource respondsToSelector:@selector(listView:contentVersionForItemAtIndex:)]) {
    for (std::size_t item : _contentVersions.update(nextItems, [self contentVersionsOfItems:nextItems.size()])) {
      [reloaded addIndex:item];
      reloadedKeys.insert(nextItems[item]);
    }
  }

  [self commitKeyChanges:std::move(next) reloadedKeys:reloadedKeys payload:nil reloadsSectionIndex:YES];

  NSMutableIndexSet *deleted = [NSMutableIndexSet indexSet];
  NSMutableIndexSet *inserted = [NSMutableIndexSet indexSet];
  NSMutableArray<NSNumber *> *movedFrom = [NSMutableArray arrayWithCapacity:diff.moved.size()];
  NSMutableArray<NSNumber *> *movedTo = [NSMutableArray arrayWithCapacity:diff.moved.size()];
  for (std::size_t index : diff.deleted) {
    [deleted addIndex:index];
  }
  for (std::size_t index : diff.inserted) {
    [inserted addIndex:index];
  }
  for (const KeyMove& move : diff.moved) {
    [movedFrom addObject:@(move.from)];
    [movedTo addObject:@(move.to)];
  }
  return [[ShadowListKitListChanges alloc] initWithDeleted:deleted inserted:inserted movedFrom:movedFrom movedTo:movedTo
                                        reloaded:reloaded];
}

/*
 * The keys a reload removes and adds, for the change animation: the changed middle between the
 * unchanged rows at both ends. A key on both sides moved. Only computed when it runs.
 */
- (void)captureChangeTo:(const std::vector<std::string>&)next
{
  const std::vector<std::string>& previous = _driver.getKeys();
  KeySplice splice = keySplice(previous, next);
  auto removedFrom = previous.begin() + (std::ptrdiff_t)splice.start;
  auto insertedFrom = next.begin() + (std::ptrdiff_t)splice.start;
  [_changes captureRemoved:std::vector<std::string>(removedFrom, removedFrom + (std::ptrdiff_t)splice.removed)
                  inserted:std::vector<std::string>(insertedFrom, insertedFrom + (std::ptrdiff_t)splice.added)];
}

/*
 * The data changed. Sticky rows follow the sections, the selection drops removed rows and a
 * waiting saved position lands once its row is there.
 */
- (void)structureChanged
{
  ++_structureVersion;
  // An open row closes. One swiped all the way stays out while its removal runs.
  if ([self isSwipeOpen] && ![self isSwipedOutCell:_swipeCell]) {
    [self closeSwipeAnimated:NO];
  }
  if (_stickyIndices.count > 0 || _stickySectionHeaders) {
    [self updateStickyRows];
  }
  if (!_selection.isEmpty()) {
    _selection.retainKeys(_driver.getKeys());
  }
  [self restorePendingAnchor];
  [self invalidateFrame];
}

/*
 * Lay out a data change right away while the list is on screen, the way UITableView applies
 * its updates. Until the pass runs, the content offset is the one from before the change and
 * misses the correction that keeps the visible rows still. UIKit code that reads it in the same
 * turn writes that offset back after the pass, and the rows jump. UIRefreshControl does this
 * in endRefreshing, called right after the refreshed rows were inserted.
 */
- (void)layoutDataChange
{
  if (!self.window || _inLayoutSubviews || _batchDepth > 0) {
    return;
  }
  [self layoutIfNeeded];
}

@end
#pragma clang diagnostic pop
