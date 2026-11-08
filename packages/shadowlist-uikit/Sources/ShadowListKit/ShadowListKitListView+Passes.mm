#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitChangeAnimator.h"
#import "Internal/ShadowListKitListSupport.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewPassesLink = 0;

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * The layout pass both kits run: the core's passes and their result, the cells of the rows with
 * their reuse, mounting near the viewport, prefetching and measurement, the separators, the
 * reached callbacks and the queries about the cells.
 */
@implementation ShadowListKitListView (Passes)

#pragma mark - Cells

- (void)registerClass:(Class)cellClass forCellReuseIdentifier:(NSString *)identifier
{
  _cellClasses[identifier] = cellClass;
}

- (__kindof ShadowListKitListCell *)dequeueReusableCellWithIdentifier:(NSString *)identifier
{
  NSMutableArray<ShadowListKitListCell *> *pool = _reusePool[identifier];
  ShadowListKitListCell *cell = pool.lastObject;
  if (cell) {
    [pool removeLastObject];
    [cell prepareForReuse];
    return cell;
  }
  Class cellClass = _cellClasses[identifier] ?: [ShadowListKitListCell class];
  cell = [[cellClass alloc] initWithReuseIdentifier:identifier];
  cell.hidden = YES;
  [[self cellContainer] addSubview:cell];
  return cell;
}

- (void)recycleCell:(ShadowListKitListCell *)cell
{
  // A cell only measured was never displayed.
  NSInteger index = cell.mountGeneration != 0 ? cell.index : NSNotFound;
  cell.mountGeneration = 0;
  [self clearCellForReuse:cell];
  [self setCell:cell hidden:YES];
  cell.index = NSNotFound;
  cell.row = NSNotFound;
  cell.separatorLayer.hidden = YES;
  if (cell.isHighlighted) {
    [cell setHighlighted:NO animated:NO];
  }
  if (cell.isSelected) {
    [cell setSelected:NO animated:NO];
  }
  if (index != NSNotFound && [_userDelegate respondsToSelector:@selector(listView:didEndDisplayingCell:atIndex:)]) {
    [_userDelegate listView:self didEndDisplayingCell:cell atIndex:index];
  }
  if (!cell.reuseIdentifier) {
    [cell removeFromSuperview];
    return;
  }
  NSMutableArray<ShadowListKitListCell *> *pool = _reusePool[cell.reuseIdentifier];
  if (!pool) {
    pool = [NSMutableArray new];
    _reusePool[cell.reuseIdentifier] = pool;
  }
  [pool addObject:cell];
}

/*
 * The cell of a row: an item's from the data source, or a section header or footer.
 */
- (ShadowListKitListCell *)makeCellAtRow:(NSInteger)row
{
  RowPlace place = _sections.placeOfRow((std::size_t)row);
  ShadowListKitListCell *cell = nil;
  switch (place.kind) {
    case RowKind::Item:
      cell = [_dataSource listView:self cellForItemAtIndex:(NSInteger)place.item];
      break;
    case RowKind::Header:
    case RowKind::Footer:
      cell = [self sectionCellAtSection:(NSInteger)place.section footer:place.kind == RowKind::Footer];
      break;
  }
  ShadowListKitView *container = [self cellContainer];
  if (cell.superview != container) {
    [container addSubview:cell];
  }
  cell.row = row;
  cell.index = place.item == UNDEFINED_INDEX ? NSNotFound : (NSInteger)place.item;
  return cell;
}

- (ShadowListKitListCell *)sectionCellAtSection:(NSInteger)section footer:(BOOL)footer
{
  if (!footer && [_dataSource respondsToSelector:@selector(listView:cellForHeaderInSection:)]) {
    return [_dataSource listView:self cellForHeaderInSection:section];
  }
  if (footer && [_dataSource respondsToSelector:@selector(listView:cellForFooterInSection:)]) {
    return [_dataSource listView:self cellForFooterInSection:section];
  }
  NSString *identifier = footer ? SHADOWLIST_KIT_SECTION_FOOTER_IDENTIFIER : SHADOWLIST_KIT_SECTION_HEADER_IDENTIFIER;
  if (!_cellClasses[identifier]) {
    _cellClasses[identifier] = [ShadowListKitSectionTitleCell class];
  }
  ShadowListKitSectionTitleCell *cell = [self dequeueReusableCellWithIdentifier:identifier];
  [cell setTitle:footer ? [_dataSource listView:self titleForFooterInSection:section]
                        : [_dataSource listView:self titleForHeaderInSection:section]];
  return cell;
}

/*
 * The mounted cell of a row, or nil.
 */
- (ShadowListKitListCell *)mountedCellAtIndex:(std::size_t)index
{
  if (index >= _driver.getKeyCount()) {
    return nil;
  }
  auto mounted = _mounted.find(_driver.getKeyAt(index));
  return mounted != _mounted.end() ? mounted->second : nil;
}

- (ShadowListKitListCell *)mountedCellForKey:(const std::string&)key
{
  auto mounted = _mounted.find(key);
  return mounted != _mounted.end() ? mounted->second : nil;
}

#pragma mark - Layout

/*
 * Run the core on the next layout pass even inside the band.
 */
- (void)invalidateFrame
{
  _needsFrame = YES;
  ShadowListKitSetNeedsLayout(self);
}

- (ListSettings)listSettings
{
  ListSettings settings;
  settings.estimatedRowSize = self.estimatedItemSize;
  settings.overscan = self.overscan;
  settings.startReachedThreshold = self.startReachedThreshold;
  settings.endReachedThreshold = self.endReachedThreshold;
  settings.numberOfColumns = (std::size_t)self.numberOfColumns;
  settings.inverted = self.inverted;
  settings.followAppends = self.followAppends;
  settings.horizontal = _horizontal;
  settings.snapToItem = self.snapToItem;
  settings.snapAlignment = static_cast<SnapAlignment>(self.snapAlignment);
  return settings;
}

/*
 * Read the viewport and template sizes. Returns whether any changed since the last pass. A new
 * cross size drops every size and keeps the first visible row, a live resize on the Mac
 * included.
 */
- (BOOL)readGeometry
{
  CGSize bounds = self.bounds.size;
  ShadowListKitEdgeInsets inset = self.adjustedContentInset;
  CGFloat along = _horizontal ? bounds.width - inset.left - inset.right : bounds.height - inset.top - inset.bottom;
  CGFloat cross = _horizontal ? bounds.height - inset.top - inset.bottom : bounds.width - inset.left - inset.right;
  if (cross != _windowCross && _windowCross > 0) {
    [self resetKeepingPosition];
  }
  CGFloat header = [self templateSize:_headerView cross:cross];
  CGFloat footer = [self templateSize:_footerView cross:cross];
  BOOL changed = along != _windowAlong || cross != _windowCross || header != _headerSize || footer != _footerSize;
  _windowAlong = along;
  _windowCross = cross;
  _headerSize = header;
  _footerSize = footer;
  return changed;
}

- (void)trackUserScroll
{
  double current = [self offset];
  if (std::fabs(current - _previousOffset) < OFFSET_MOVED_THRESHOLD) {
    return;
  }
  if ([self corePhase] != ScrollPhase::Idle) {
    _userScrolled = YES;
  }
  _previousOffset = current;
}

/*
 * Run the core until the measured range has sizes and any correction landed. Everything happens
 * before the frame is drawn. The reader never sees an estimate or a correction.
 */
- (void)runPasses
{
  ListSettings settings = [self listSettings];
  if (!_hasSentSettings || settings != _sentSettings) {
    _driver.setSettings(settings);
    _sentSettings = settings;
    _hasSentSettings = YES;
  }
  PassInput input;
  input.offset = [self offset];
  input.windowAlong = _windowAlong;
  input.windowCross = _windowCross;
  input.headerSize = _headerSize;
  input.footerSize = _footerSize;
  input.phase = [self corePhase];
  input.userScrolled = _userScrolled;
  input.tracking = self.isTracking;
  // The mount pass's padding, which the core prefetches around before it measures.
  input.prefetch = self.prefetchDataSource != nil;
  input.mountPad = _windowAlong * self.mountOverscan;
  input.leadingInset = [self leadingInset];
  input.trailingInset = [self trailingInset];
  PassResult result = _driver.runPasses(input);
  ++_corePasses;
  _userScrolled = NO;
  _needsFrame = NO;
  [self applyPassResult:result];
}

/*
 * The content size goes first. The offset write is then inside the scroll range.
 */
- (void)applyPassResult:(const PassResult&)result
{
  [self applyContentSize:(CGFloat)result.contentAlong];
  /*
   * Content that shrank under a resting view clamps it to the new end. The core leaves that
   * to the host for inverted lists. It runs again on the clamped offset before the frame is
   * drawn. A drag or a bounce past the end is left to the gesture.
   */
  if (result.offset > [self maxOffset] + 0.5 && [self corePhase] == ScrollPhase::Idle && !_clampingPass) {
    [self writeOffset:[self maxOffset] byUser:NO];
    _clampingPass = YES;
    [self runPasses];
    _clampingPass = NO;
    return;
  }
  if (std::fabs(result.offset - [self offset]) >= 0.01) {
    [self writeOffset:result.offset byUser:NO];
  }
  _band = result.band;
  _footerStart = result.footerStart;
  _reachedStart = _reachedStart || result.reachedStart;
  _reachedEnd = _reachedEnd || result.reachedEnd;
  if (result.settling) {
    [self scheduleSettleFrame];
  }
}

/*
 * A correction waits for one more report. Give it the next display frame, once.
 */
- (void)scheduleSettleFrame
{
  _needsFrame = YES;
  if (_settleScheduled) {
    return;
  }
  _settleScheduled = YES;
  if (!_settleLink) {
    ShadowListKitSettleLinkTarget *target = [ShadowListKitSettleLinkTarget new];
    target.list = self;
    _settleLink = ShadowListKitDisplayLink(self, target, @selector(tick:));
    [_settleLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
  }
  _settleLink.paused = NO;
}

- (void)settleFrame
{
  _settleLink.paused = YES;
  _settleScheduled = NO;
  ShadowListKitSetNeedsLayout(self);
  ShadowListKitLayoutNow(self);
}

/*
 * Drop all sizes after a cross size change and hold the first visible row.
 */
- (void)resetKeepingPosition
{
  _driver.resetKeepingPosition();
  _band = OffsetBand();
  _needsFrame = YES;
  ++_structureVersion;
}

#pragma mark - Mounting

- (CGRect)rowRect:(std::size_t)index
{
  RowRect rect = _driver.getRowRect(index);
  return [self displayRect:rect.x y:rect.y width:rect.width height:rect.height];
}

/*
 * A frame in the content as drawn: moved by the pixel shift and with its edges on device
 * pixels. Rows that touch in the core still touch.
 */
- (CGRect)displayRect:(double)x y:(double)y width:(double)width height:(double)height
{
  double shift = [self pixelShift];
  double shiftX = _horizontal ? shift : 0.0;
  double shiftY = _horizontal ? 0.0 : shift;
  CGFloat scale = [self displayScale];
  auto aligned = [scale](double value) { return (CGFloat)(std::round(value * scale) / scale); };
  CGFloat left = aligned(x - shiftX);
  CGFloat top = aligned(y - shiftY);
  CGFloat right = aligned(x + width - shiftX);
  CGFloat bottom = aligned(y + height - shiftY);
  return CGRectMake(left, top, right - left, bottom - top);
}

/*
 * Mount the cells of rows near the viewport and recycle the rest. Rows are matched by key,
 * which keeps the content of a cell that only moved. Rows under the insets, like translucent
 * bars, show and are mounted too.
 */
- (void)mountCells
{
  if (!_driver.getMeasuredRange()) {
    [self unmountAll];
    return;
  }
  MountPlan plan = _driver.planMount([self offset], _windowAlong, _windowAlong * self.mountOverscan, [self leadingInset],
    [self trailingInset]);
  _mountBand = plan.band;
  if (![self mountNeeded:plan]) {
    return;
  }
  [self recordMount:plan];
  [self mountPlan:plan];
  [self prefetchAround:plan];
}

/*
 * Whether anything the last mount pass covered changed: the rows, the pinned header, the
 * geometry, the data or the pixel shift. A drag mounts every pass.
 */
- (BOOL)mountNeeded:(const MountPlan&)plan
{
  return plan.low != _mountedLow || plan.high != _mountedHigh || plan.sticky != _mountedSticky ||
    _driver.getGeometryVersion() != _mountedGeometry || _structureVersion != _mountedStructure ||
    [self pixelShift] != _mountedShift || [self hasHeldRow];
}

- (void)recordMount:(const MountPlan&)plan
{
  _mountedLow = plan.low;
  _mountedHigh = plan.high;
  _mountedSticky = plan.sticky;
  _mountedGeometry = _driver.getGeometryVersion();
  _mountedStructure = _structureVersion;
  _mountedShift = [self pixelShift];
}

- (void)mountPlan:(const MountPlan&)plan
{
  ++_mountPasses;
  NSUInteger generation = ++_mountGeneration;
  for (std::size_t index = plan.low; plan.low != UNDEFINED_INDEX && index <= plan.high; ++index) {
    if (_driver.shouldMount(plan, index)) {
      [self mountRow:index generation:generation];
    }
  }
  if (plan.sticky != UNDEFINED_INDEX) {
    [self mountRow:plan.sticky generation:generation];
  }
  // The held row stays mounted wherever the finger or the mouse takes it.
  std::size_t held = _driver.getHeldIndex();
  if (held != UNDEFINED_INDEX) {
    [self mountRow:held generation:generation];
  }
  [self recycleStaleCells:generation];
}

- (void)mountRow:(std::size_t)index generation:(NSUInteger)generation
{
  if (index >= _driver.getRowCount() || index >= _driver.getKeyCount()) {
    return;
  }
  const std::string& key = _driver.getKeyAt(index);
  ShadowListKitListCell *cell = nil;
  BOOL appearing = NO;
  auto mounted = _mounted.find(key);
  if (mounted != _mounted.end()) {
    cell = mounted->second;
    appearing = cell.mountGeneration == 0;
  } else {
    cell = [self makeCellAtRow:(NSInteger)index];
    _mounted.emplace(key, cell);
    appearing = YES;
  }
  std::size_t item = _sections.itemForRow(index);
  cell.row = (NSInteger)index;
  cell.index = item == UNDEFINED_INDEX ? NSNotFound : (NSInteger)item;
  cell.mountGeneration = generation;
  [self placeCell:cell frame:[self rowRect:index]];
  [self placeSeparatorOfCell:cell row:index];
  if (!appearing) {
    return;
  }
  [self setCell:cell hidden:NO];
  [self applyStateToCell:cell key:key];
  // A reload recycled the held row's cell. The new one takes its place under the finger or the
  // mouse.
  if (index == _driver.getHeldIndex()) {
    [self adoptHeldCell:cell];
  }
  if (cell.index != NSNotFound && [_userDelegate respondsToSelector:@selector(listView:willDisplayCell:atIndex:)]) {
    [_userDelegate listView:self willDisplayCell:cell atIndex:cell.index];
  }
}

- (void)recycleStaleCells:(NSUInteger)generation
{
  for (auto entry = _mounted.begin(); entry != _mounted.end();) {
    if (entry->second.mountGeneration != generation) {
      // A cell measured ahead keeps its configuration until its row comes into view.
      if (entry->second.mountGeneration == 0 && [self keptAfterMeasure:entry->second key:entry->first]) {
        ++entry;
        continue;
      }
      // A deleted row's cell fades out first, then goes back to the pool. One swiped out does not.
      if ([self isSwipedOutCell:entry->second] || ![_changes fadeOutKey:entry->first cell:entry->second]) {
        [self recycleCell:entry->second];
      }
      entry = _mounted.erase(entry);
    } else {
      ++entry;
    }
  }
}

- (BOOL)keptAfterMeasure:(ShadowListKitListCell *)cell key:(const std::string&)key
{
  std::optional<MountedRange> measured = _driver.getMeasuredRange();
  if (!measured || cell.row == NSNotFound) {
    return NO;
  }
  std::size_t row = (std::size_t)cell.row;
  return row >= measured->low && row <= measured->high && row < _driver.getKeyCount() && _driver.getKeyAt(row) == key;
}

- (void)unmountAll
{
  for (auto& entry : _mounted) {
    [self recycleCell:entry.second];
  }
  _mounted.clear();
  _mountBand = OffsetBand();
  _mountedLow = UNDEFINED_INDEX;
  _mountedHigh = UNDEFINED_INDEX;
  _mountedSticky = UNDEFINED_INDEX;
}

#pragma mark - Prefetching

/*
 * Tell the prefetch data source about items the measured range brought in that are not
 * mounted, and the ones that left it unseen. runPasses already told it about the rows it
 * brought in before it measured them, which is before their cells exist. This finds what the
 * mount changed.
 */
- (void)prefetchAround:(const MountPlan&)plan
{
  if (!self.prefetchDataSource) {
    return;
  }
  _driver.updatePrefetch(plan.low, plan.high, _prefetchRows, _cancelRows);
  [self reportPrefetchRows:_prefetchRows cancel:_cancelRows];
}

/*
 * Hand prefetch changes to the prefetch data source as items.
 */
- (void)reportPrefetchRows:(const std::vector<std::size_t>&)rows cancel:(const std::vector<std::size_t>&)cancelRows
{
  id<ShadowListKitListViewPrefetchDataSource> prefetch = self.prefetchDataSource;
  if (!prefetch) {
    return;
  }
  NSIndexSet *items = [self itemsOfRows:rows];
  if (items.count > 0) {
    [prefetch listView:self prefetchItemsAtIndices:items];
  }
  NSIndexSet *cancelled = [self itemsOfRows:cancelRows];
  if (cancelled.count > 0 && [prefetch respondsToSelector:@selector(listView:cancelPrefetchingForItemsAtIndices:)]) {
    [prefetch listView:self cancelPrefetchingForItemsAtIndices:cancelled];
  }
}

- (NSIndexSet *)itemsOfRows:(const std::vector<std::size_t>&)rows
{
  NSMutableIndexSet *items = [NSMutableIndexSet indexSet];
  for (std::size_t item : _sections.itemsOfRows(rows)) {
    [items addIndex:item];
  }
  return items;
}

#pragma mark - Measurement

/*
 * Called by the core for every row in its measured range that has no size yet. Reads the size from
 * the data source or measures the row through its cell.
 */
- (CGFloat)measureRow:(NSInteger)row key:(const std::string&)key cross:(CGFloat)cross
{
  RowPlace place = _sections.placeOfRow((std::size_t)row);
  switch (place.kind) {
    case RowKind::Item:
      if (_sizesFromDataSource) {
        return [_dataSource listView:self sizeForItemAtIndex:(NSInteger)place.item crossSize:cross];
      }
      break;
    case RowKind::Header:
      if ([_dataSource respondsToSelector:@selector(listView:sizeForHeaderInSection:crossSize:)]) {
        return [_dataSource listView:self sizeForHeaderInSection:(NSInteger)place.section crossSize:cross];
      }
      break;
    case RowKind::Footer:
      if ([_dataSource respondsToSelector:@selector(listView:sizeForFooterInSection:crossSize:)]) {
        return [_dataSource listView:self sizeForFooterInSection:(NSInteger)place.section crossSize:cross];
      }
      break;
  }
  return [self measureCellAtRow:row key:key cross:cross];
}

/*
 * Measure a row through its cell. The cell is mounted right after when the row is in view,
 * which rarely wastes the configure work.
 */
- (CGFloat)measureCellAtRow:(NSInteger)row key:(const std::string&)key cross:(CGFloat)cross
{
  ShadowListKitListCell *cell = nil;
  auto mounted = _mounted.find(key);
  if (mounted != _mounted.end()) {
    cell = mounted->second;
  } else {
    cell = [self makeCellAtRow:row];
    cell.mountGeneration = 0;
    _mounted.emplace(key, cell);
  }
  CGSize fits = [cell sizeThatFits:_horizontal ? CGSizeMake(CGFLOAT_MAX, cross) : CGSizeMake(cross, CGFLOAT_MAX)];
  return _horizontal ? fits.width : fits.height;
}

#pragma mark - Separators

/*
 * The line over an item's trailing edge when another item of its section follows. Grids have
 * none.
 */
- (void)placeSeparatorOfCell:(ShadowListKitListCell *)cell row:(std::size_t)row
{
  BOOL shows = self.showsSeparators && self.numberOfColumns == 1 && row + 1 < _driver.getKeyCount() &&
    _sections.isItemBeforeItem(row);
  if (shows && cell.index != NSNotFound && [_userDelegate respondsToSelector:@selector(listView:showsSeparatorAfterItemAtIndex:)]) {
    shows = [_userDelegate listView:self showsSeparatorAfterItemAtIndex:cell.index];
  }
  if (!shows) {
    cell.separatorLayer.hidden = YES;
    return;
  }
  CALayer *line = cell.separatorLayer;
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  if (!line) {
    line = [CALayer layer];
    line.zPosition = 1000;
    cell.separatorLayer = line;
    [cell.layer addSublayer:line];
  }
  CGFloat thickness = self.separatorThickness > 0 ? self.separatorThickness : 1 / [self displayScale];
  CGSize size = cell.bounds.size;
  CGRect frame = _horizontal
    ? CGRectMake(size.width - thickness, self.separatorInsetStart, thickness,
        MAX(0, size.height - self.separatorInsetStart - self.separatorInsetEnd))
    : CGRectMake(self.separatorInsetStart, size.height - thickness,
        MAX(0, size.width - self.separatorInsetStart - self.separatorInsetEnd), thickness);
  line.hidden = NO;
  line.frame = frame;
  line.backgroundColor = [self separatorCGColor];
  [CATransaction commit];
}

#pragma mark - Events

- (void)dispatchReached
{
  if (_reachedStart) {
    _reachedStart = NO;
    if ([_userDelegate respondsToSelector:@selector(listViewDidReachStart:)]) {
      [_userDelegate listViewDidReachStart:self];
    }
  }
  if (_reachedEnd) {
    _reachedEnd = NO;
    if ([_userDelegate respondsToSelector:@selector(listViewDidReachEnd:)]) {
      [_userDelegate listViewDidReachEnd:self];
    }
  }
}

#pragma mark - Queries

/*
 * The Mac's views take their layers' geometry first. A caller reads their frames.
 */
- (ShadowListKitListCell *)cellForItemAtIndex:(NSInteger)index
{
#if TARGET_OS_OSX
  [self catchUpViews];
#endif
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound) {
    return nil;
  }
  ShadowListKitListCell *cell = [self mountedCellAtIndex:(std::size_t)row];
  return cell.hidden ? nil : cell;
}

- (NSArray<ShadowListKitListCell *> *)visibleCells
{
#if TARGET_OS_OSX
  [self catchUpViews];
#endif
  NSMutableArray<ShadowListKitListCell *> *cells = [NSMutableArray arrayWithCapacity:_mounted.size()];
  CGRect visible = [self cellContainer].bounds;
  for (auto& entry : _mounted) {
    ShadowListKitListCell *cell = entry.second;
    if (!cell.hidden && cell.index != NSNotFound && CGRectIntersectsRect(cell.frame, visible)) {
      [cells addObject:cell];
    }
  }
  [cells sortUsingComparator:^NSComparisonResult(ShadowListKitListCell *a, ShadowListKitListCell *b) {
    return a.row < b.row ? NSOrderedAscending : a.row > b.row ? NSOrderedDescending : NSOrderedSame;
  }];
  return cells;
}

/*
 * The items among the rows overlapping the viewport. Headers and footers do not count.
 */
- (NSRange)visibleRange
{
  std::optional<MountedRange> visible = _driver.getVisibleRange();
  std::optional<MountedRange> items = visible ? _sections.itemRangeOfRows(visible->low, visible->high) : std::nullopt;
  if (!items) {
    return NSMakeRange(NSNotFound, 0);
  }
  return NSMakeRange(items->low, items->high - items->low + 1);
}

- (CGRect)rectForItemAtIndex:(NSInteger)index
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound || (std::size_t)row >= _driver.getRowCount()) {
    return CGRectNull;
  }
  return [self rowRect:(std::size_t)row];
}

@end
#pragma clang diagnostic pop
