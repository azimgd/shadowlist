#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitItemSizeSpec.h"
#import "Internal/ShadowListKitViewabilityConfig.h"
#import "Internal/ShadowListKitChangeAnimator.h"
#import "Internal/ShadowListKitListSupport.h"

#include <shadowlist-core/host/StickyLayout.hpp>

#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace azimgd::shadowlist;

static_assert(static_cast<int>(ShadowListKitSnapAlignmentStart) == static_cast<int>(SnapAlignment::Start) &&
                static_cast<int>(ShadowListKitSnapAlignmentCenter) == static_cast<int>(SnapAlignment::Center) &&
                static_cast<int>(ShadowListKitSnapAlignmentEnd) == static_cast<int>(SnapAlignment::End),
  "ShadowListKitSnapAlignment mirrors the core's SnapAlignment");

/*
 * How far past the end of its content a scroll view rests on its own, on the device pixel it
 * rounds to.
 */
static const double SHADOWLIST_KIT_END_SLACK = 0.5;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewPassesLink = 0;

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * The layout pass both kits run: the core's passes and their result, the pinned sticky row, the
 * cells of the rows with their reuse, mounting near the viewport, prefetching and measurement,
 * the separators, the reached callbacks and the queries about the cells.
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
  if (cell == _stickyHeaderOverlayCell) [self clearStickyHeaderOverlay];
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
  CGFloat empty = [self templateSize:_emptyView cross:cross];
  BOOL changed = along != _windowAlong || cross != _windowCross || header != _headerSize || footer != _footerSize ||
    empty != _emptySize;
  _windowAlong = along;
  _windowCross = cross;
  _headerSize = header;
  _footerSize = footer;
  _emptySize = empty;
  return changed;
}

- (void)trackUserScroll
{
  double current = [self offset];
  if (std::fabs(current - _previousOffset) < OFFSET_MOVED_THRESHOLD) {
    return;
  }
  BOOL byUser = [self corePhase] != ScrollPhase::Idle;
  [self updateStickyOffset:current byUser:byUser];
  if (byUser) {
    _driver.recordInteraction(CACurrentMediaTime() * 1000);
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
  input.emptySize = _emptySize;
  input.phase = [self corePhase];
  input.userScrolled = _userScrolled;
  input.tracking = self.isTracking;
  // The mount pass's padding, which the core prefetches around before it measures.
  input.prefetch = self.prefetchDataSource != nil;
  input.mountPad = _windowAlong * self.mountOverscan;
  input.leadingInset = [self leadingInset];
  input.trailingInset = [self trailingInset];
  /*
   * Content that shrank under a resting view leaves it past the new end, and the driver brings
   * it back before the frame is drawn. A drag or a bounce past the end is left to the gesture,
   * and the start to the scroll view.
   */
  input.clampEnd = input.phase == ScrollPhase::Idle;
  input.clampSlack = SHADOWLIST_KIT_END_SLACK;
  PassResult result = _driver.runPasses(input);
  ++_corePasses;
  _userScrolled = NO;
  _needsFrame = NO;
  [self applyPassResult:result];
}

/*
 * The content size goes first. The offset write is then inside the scroll range. The driver
 * hands an edge it reached over once, and says after every pass whether the list is at its end.
 */
- (void)applyPassResult:(const PassResult&)result
{
  [self applyContentSize:(CGFloat)result.contentAlong];
  _reachedStart = _reachedStart || result.reachedStart;
  _reachedEnd = _reachedEnd || result.reachedEnd;
  if (result.atEnd != _atEnd) {
    _atEnd = result.atEnd;
    _atEndChanged = !_atEndChanged;
  }
  _showsEmpty = result.showsEmpty;
  if (std::fabs(result.offset - [self offset]) >= 0.01) {
    [self writeOffset:result.offset byUser:NO];
  }
  _band = result.band;
  _footerStart = result.footerStart;
  if (result.settling) {
    [self scheduleSettleFrame];
  }
}

/*
 * A correction waits for one more report, or the end moved under a list the driver brought back
 * to it. Give it the next display frame, once.
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

/*
 * One pass runs the core when the offset left the band, then mounts. Everything lands
 * before the frame is drawn. A pass asked for during a pass, or an offset a delegate wrote
 * while the cells mounted, lays out once more on the next layout.
 */
- (void)layoutPass
{
  if (_inLayoutPass) {
    _layoutPassRequested = YES;
    return;
  }
  _inLayoutPass = YES;
  ++_layoutPasses;
#if TARGET_OS_OSX
  if (_viewsBehind && ![self placesByLayer]) {
    [self catchUpViews];
  }
#endif
  BOOL geometryChanged = [self readGeometry];
#if TARGET_OS_OSX
  [self placeContentView];
#endif
  [self trackUserScroll];
  if (_windowCross > 0 && _windowAlong > 0) {
    if (_needsFrame || geometryChanged || !_band.contains([self offset])) {
      [self runPasses];
    }
    [self readPassGeometry];
    double mountedOffset = [self offset];
    [self layoutTemplates];
    [self mountCells];
    [self layoutSticky];
    if ([self hasHeldRow]) {
      [self updateDrag];
    }
    [_changes run];
    [self layoutSwipe];
#if !TARGET_OS_OSX
    [self layoutSectionIndex];
#endif
    if ([self offset] != mountedOffset) {
      _layoutPassRequested = YES;
    }
  }
  _passGeometry.valid = false;
  _inLayoutPass = NO;
  if (_layoutPassRequested) {
    _layoutPassRequested = NO;
    ShadowListKitSetNeedsLayout(self);
  }
#if TARGET_OS_OSX
  [self scrollGeometryChanged];
  // The content may have moved under a still pointer.
  if (_hoverActive || _hoveredCell) {
    [self updateHover];
  }
#endif
  [self dispatchReached];
  [self updateViewability];
#if TARGET_OS_OSX
  if ([_userDelegate respondsToSelector:@selector(listViewDidLayOut:)]) {
    [_userDelegate listViewDidLayOut:self];
  }
#endif
}

/*
 * Place the header, the empty view and the footer. A pinned header or footer moves by the host
 * layer's translation on every pass and sits over the rows. The empty view shows right after the
 * header while the core says the list has no rows.
 */
- (StickyInput)stickyInput
{
  StickyInput sticky;
  sticky.offset = [self offset];
  sticky.windowSize = _windowAlong;
  sticky.contentSize = _contentAlong;
  sticky.hasHeader = _headerView != nil;
  sticky.headerSize = _headerSize;
  sticky.stickyHeader = _stickyHeader;
  sticky.autoHideHeader = _autoHideHeader;
  sticky.hasFooter = _footerView != nil;
  sticky.footerSize = _footerSize;
  sticky.footerStart = _footerStart;
  sticky.stickyFooter = _stickyFooter;
  sticky.autoHideFooter = _autoHideFooter;
  return sticky;
}

- (void)updateStickyOffset:(double)offset byUser:(BOOL)byUser
{
  StickyInput sticky = [self stickyInput];
  sticky.offset = offset;
  sticky.accumulate = byUser;
  _driver.stickyTranslations(sticky);
}

- (void)layoutTemplates
{
  StickyTranslations translations = _driver.stickyTranslations([self stickyInput]);
  if (_headerView) {
    [self placeTemplate:_headerView start:translations.header size:_headerSize pinned:(_stickyHeader || _autoHideHeader)];
  }
  if (_emptyView) {
    if (_emptyView.hidden == _showsEmpty) {
      _emptyView.hidden = !_showsEmpty;
    }
    [self placeTemplate:_emptyView start:_headerSize size:_emptySize pinned:NO];
  }
  if (_footerView) {
    [self placeTemplate:_footerView start:_footerStart + translations.footer size:_footerSize pinned:(_stickyFooter || _autoHideFooter)];
  }
}

- (void)placeTemplate:(ShadowListKitView *)view start:(double)start size:(CGFloat)size pinned:(BOOL)pinned
{
  CGRect frame = _horizontal ? [self displayRect:start y:0 width:size height:_windowCross]
                             : [self displayRect:0 y:start width:_windowCross height:size];
  ShadowListKitPlace(view, frame);
#if TARGET_OS_OSX
  // A view sits over its siblings by its own layer.
  if (pinned && !view.wantsLayer) {
    view.wantsLayer = YES;
  }
#endif
  CGFloat zPosition = pinned ? 1 : 0;
  if (view.layer.zPosition != zPosition) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    view.layer.zPosition = zPosition;
    [CATransaction commit];
  }
}

- (ShadowListKitView *)shownEmptyView
{
  return _showsEmpty ? _emptyView : nil;
}

- (BOOL)isUnderPinnedTemplate:(CGPoint)point
{
  return (_stickyHeaderOverlay && CGRectContainsPoint(_stickyHeaderOverlay.frame, point)) ||
    ((_stickyHeader || _autoHideHeader) && _headerView && CGRectContainsPoint(_headerView.frame, point)) ||
    ((_stickyFooter || _autoHideFooter) && _footerView && CGRectContainsPoint(_footerView.frame, point));
}

- (void)sendSizeSpecSource
{
  if (!_getItemSizeSpec) {
    _driver.setGetItemSizeSpec({}, {});
  } else {
    __weak ShadowListKitListView *weakSelf = self;
    _driver.setGetItemSizeSpec([weakSelf](std::size_t row, const std::string&) -> std::optional<RowSizeSpec> {
      ShadowListKitListView *list = weakSelf;
      if (!list || !list->_getItemSizeSpec) return std::nullopt;
      std::size_t item = list->_sections.itemForRow(row);
      if (item == UNDEFINED_INDEX) return std::nullopt;
      ShadowListKitItemSizeSpec *value = list->_getItemSizeSpec((NSInteger)item);
      return value ? std::optional<RowSizeSpec>(ShadowListKitReadItemSizeSpec(value)) : std::nullopt;
    }, ShadowListKitMeasureItemSizeSpec);
  }
  [self invalidateFrame];
}

- (void)sendPersistentKeys
{
  std::vector<std::string> keys;
  keys.reserve(_persistentKeys.count);
  for (NSString *key in _persistentKeys) {
    keys.push_back(ShadowListKitStdString(key));
  }
  _driver.setPersistentKeys(std::move(keys));
  ++_structureVersion;
  [self invalidateFrame];
}

- (void)sendNonAnchorKeys
{
  std::vector<std::string> keys;
  keys.reserve(_nonAnchorKeys.count);
  for (NSString *key in _nonAnchorKeys) {
    keys.push_back(ShadowListKitStdString(key));
  }
  _driver.setNonAnchorKeys(std::move(keys));
  [self invalidateFrame];
}

#pragma mark - Viewability

- (void)sendViewabilityConfigs
{
  [_viewabilityTimer invalidate];
  _viewabilityTimer = nil;
  ++_viewabilityGeneration;
  if (_viewabilityConfigCallbackPairs) {
    _viewabilityCallbacks = [_viewabilityConfigCallbackPairs copy];
  } else if (_onViewableItemsChanged) {
    ShadowListKitViewabilityConfig *config = _viewabilityConfig ?: [ShadowListKitViewabilityConfig new];
    _viewabilityCallbacks = @[[[ShadowListKitViewabilityConfigCallbackPair alloc]
      initWithViewabilityConfig:config onViewableItemsChanged:_onViewableItemsChanged]];
  } else {
    _viewabilityCallbacks = @[];
  }
  std::vector<ViewabilityConfig> configs;
  for (ShadowListKitViewabilityConfigCallbackPair *pair in _viewabilityCallbacks) {
    configs.push_back(ShadowListKitReadViewabilityConfig(pair.viewabilityConfig));
  }
  auto changed = _driver.setViewabilityConfigs(std::move(configs), CACurrentMediaTime() * 1000);
  if (!_viewabilityTokens) _viewabilityTokens = [NSMutableArray new];
  while (_viewabilityTokens.count > _viewabilityCallbacks.count) [_viewabilityTokens removeLastObject];
  while (_viewabilityTokens.count < _viewabilityCallbacks.count) [_viewabilityTokens addObject:[NSMutableDictionary new]];
  for (std::size_t index : changed) _viewabilityTokens[index] = [NSMutableDictionary new];
  [self updateViewability];
}

- (void)recordInteraction
{
  _driver.recordInteraction(CACurrentMediaTime() * 1000);
  [self updateViewability];
}

- (void)detachViewability
{
  _viewabilityAttached = NO;
  ++_viewabilityGeneration;
  [_viewabilityTimer invalidate];
  _viewabilityTimer = nil;
  _driver.resetViewability();
  for (NSMutableDictionary *tokens in _viewabilityTokens) [tokens removeAllObjects];
}

- (void)updateViewability
{
  if (_viewabilityUpdating) { _viewabilityNeedsUpdate = YES; return; }
  if (!_viewabilityAttached || !self.window || _viewabilityCallbacks.count == 0) return;
  _viewabilityUpdating = YES;
  _viewabilityNeedsUpdate = NO;
  NSUInteger generation = _viewabilityGeneration;
  @try {
    auto reports = _driver.updateViewability([self offset], _windowAlong, CACurrentMediaTime() * 1000, &_sections);
    NSMutableDictionary<NSNumber *, ShadowListKitViewableItemsChangedInfo *> *events = [NSMutableDictionary new];
    for (auto& entry : reports) {
      std::size_t config = entry.first;
      if (config >= _viewabilityTokens.count) continue;
      auto& report = entry.second;
      NSMutableDictionary<NSString *, ShadowListKitViewToken *> *current = [NSMutableDictionary new];
      NSMutableArray<ShadowListKitViewToken *> *viewable = [NSMutableArray new];
      NSMutableArray<ShadowListKitViewToken *> *changed = [NSMutableArray new];
      for (const std::string& key : report.viewable) {
        std::size_t row = _driver.indexOfKey(key);
        std::size_t item = row == UNDEFINED_INDEX ? UNDEFINED_INDEX : _sections.itemForRow(row);
        if (item == UNDEFINED_INDEX) continue;
        NSString *publicKey = [_dataSource listView:self keyForItemAtIndex:(NSInteger)item];
        if (!publicKey) continue;
        NSInteger section = _sections.isSectioned() ? (NSInteger)_sections.sectionForItem(item) : NSNotFound;
        ShadowListKitViewToken *token = [[ShadowListKitViewToken alloc] initWithKey:publicKey index:(NSInteger)item section:section viewable:YES];
        current[ShadowListKitString(key)] = token;
        [viewable addObject:token];
      }
      [viewable sortUsingComparator:^NSComparisonResult(ShadowListKitViewToken *a, ShadowListKitViewToken *b) {
        return a.index < b.index ? NSOrderedAscending : a.index > b.index ? NSOrderedDescending : NSOrderedSame;
      }];
      for (const std::string& key : report.changes.added) {
        ShadowListKitViewToken *token = current[ShadowListKitString(key)];
        if (token) [changed addObject:token];
      }
      for (const std::string& key : report.changes.removed) {
        ShadowListKitViewToken *previous = _viewabilityTokens[config][ShadowListKitString(key)];
        if (previous) [changed addObject:[[ShadowListKitViewToken alloc] initWithKey:previous.key index:previous.index section:previous.section viewable:NO]];
      }
      _viewabilityTokens[config] = current;
      if (changed.count) {
        ShadowListKitViewableItemsChangedInfo *info = [[ShadowListKitViewableItemsChangedInfo alloc] initWithViewableItems:viewable changed:changed];
        events[@(config)] = info;
      }
    }
    for (const auto& entry : reports) {
      if (generation != _viewabilityGeneration || !_viewabilityAttached) break;
      NSUInteger config = entry.first;
      ShadowListKitViewableItemsChangedInfo *info = events[@(config)];
      if (!info || config >= _viewabilityCallbacks.count) continue;
      void (^callback)(ShadowListKitViewableItemsChangedInfo *) = _viewabilityCallbacks[config].onViewableItemsChanged;
      if (callback) callback(info);
    }
  } @finally {
    _viewabilityUpdating = NO;
  }
  if (_viewabilityNeedsUpdate || generation != _viewabilityGeneration) {
    [self updateViewability];
  } else if (_driver.isViewabilityWaiting() && !_viewabilityTimer && _viewabilityAttached) {
    __weak ShadowListKitListView *weakSelf = self;
    _viewabilityTimer = [NSTimer timerWithTimeInterval:0.016 repeats:NO block:^(NSTimer *timer) {
      ShadowListKitListView *list = weakSelf;
      if (!list) return;
      list->_viewabilityTimer = nil;
      [list updateViewability];
    }];
    [NSRunLoop.mainRunLoop addTimer:_viewabilityTimer forMode:NSRunLoopCommonModes];
  }
}

#pragma mark - Sticky

/*
 * Pin the active section header and put the one it replaced back in its row. A pinned row
 * sits over the rows and under a held one. Its frame pins it and leaves the transform a drag
 * shifts it by.
 */
- (void)layoutSticky
{
  if (!_driver.hasSticky()) {
    [self clearStickyHeaderOverlay];
    if (_stickyCell) {
      [self unpinCell:_stickyCell];
      _stickyCell = nil;
    }
    return;
  }
  double offset = [self offset];
  std::size_t active = _driver.activeStickyIndex(offset);
  ShadowListKitListCell *cell = active != UNDEFINED_INDEX ? [self mountedCellAtIndex:active] : nil;
  // A held sticky item follows the pointer from its own row.
  if (cell == _heldCell) {
    cell = nil;
  }
  if (_stickyCell && _stickyCell != cell) {
    [self unpinCell:_stickyCell];
  }
  _stickyCell = cell;
  if ([self layoutStickyHeaderOverlay:active cell:cell]) return;
  if (!cell || active >= _driver.getRowCount() || cell.row != (NSInteger)active) {
    return;
  }
  RowRect rect = _driver.getRowRect(active);
  double pinned = _driver.stickyLeading(active, offset);
  if (_horizontal) {
    rect.x = pinned;
  } else {
    rect.y = pinned;
  }
  [self placeCell:cell frame:[self displayRect:rect.x y:rect.y width:rect.width height:rect.height]];
  if (cell.layer.zPosition != 1) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    cell.layer.zPosition = 1;
    [CATransaction commit];
  }
}

/*
 * The original row remains mounted for identity and measurement. Hiding its view gives the
 * custom header one visual and accessibility representation; removing it restores that view.
 */
- (void)clearStickyHeaderOverlay
{
#if TARGET_OS_OSX
  [_stickyHeaderOverlay setAccessibilityParent:nil];
#endif
  [_stickyHeaderOverlay removeFromSuperview];
  _stickyHeaderOverlay = nil;
  if (_stickyHeaderOverlayCell) {
#if TARGET_OS_OSX
    [_stickyHeaderOverlayCell catchUpView];
#endif
    _stickyHeaderOverlayCell.hidden = _stickyHeaderOverlayCellHidden;
  }
  _stickyHeaderOverlayCell = nil;
  _stickyHeaderOverlayKey.clear();
  ++_stickyHeaderOverlayRevision;
}

- (void)invalidateStickyHeaderOverlay
{
  ++_stickyHeaderOverlayRevision;
}

- (BOOL)layoutStickyHeaderOverlay:(std::size_t)active cell:(ShadowListKitListCell *)cell
{
  if (!_renderStickyHeaderOverlay || !cell || active >= _driver.getKeyCount()) {
    [self clearStickyHeaderOverlay];
    return NO;
  }
  std::string key = _driver.getKeyAt(active);
  if (_stickyHeaderOverlayKey != key || _stickyHeaderOverlayRenderedRevision != _stickyHeaderOverlayRevision) {
    [self clearStickyHeaderOverlay];
    NSUInteger revision = _stickyHeaderOverlayRevision;
    RowPlace place = _sections.placeOfRow(active);
    NSInteger index = place.item == UNDEFINED_INDEX ? NSNotFound : (NSInteger)place.item;
    NSInteger section = _sections.isSectioned() ? (NSInteger)place.section : NSNotFound;
    NSString *publicKey = nil;
    if (index != NSNotFound) {
      publicKey = [_dataSource listView:self keyForItemAtIndex:index];
    } else if (section != NSNotFound) {
      if ([_dataSource respondsToSelector:@selector(listView:keyForSection:)]) {
        publicKey = [_dataSource listView:self keyForSection:section];
      }
      if (!publicKey && _sections.itemCountInSection(place.section) > 0) {
        publicKey = [_dataSource listView:self keyForItemAtIndex:(NSInteger)_sections.firstItemInSection(place.section)];
      }
    }
    ShadowListKitView * _Nullable (^render)(NSString * _Nullable, NSInteger, NSInteger) = [_renderStickyHeaderOverlay copy];
    ShadowListKitView *view = render(publicKey, index, section);
    // A factory can synchronously replace the data or itself. Its old view never lands.
    if (revision != _stickyHeaderOverlayRevision || !_renderStickyHeaderOverlay) return NO;
    _stickyHeaderOverlayKey = key;
    _stickyHeaderOverlayRenderedRevision = revision;
    _stickyHeaderOverlay = view;
    if (view) {
      _stickyHeaderOverlayCell = cell;
#if TARGET_OS_OSX
      [cell catchUpView];
#endif
      _stickyHeaderOverlayCellHidden = cell.hidden;
      cell.hidden = YES;
      [[self cellContainer] addSubview:view];
#if TARGET_OS_OSX
      [view setAccessibilityParent:(id)[self accessibilityElementForRow:active]];
#endif
    }
  }
  ShadowListKitView *view = _stickyHeaderOverlay;
  if (!view) return NO;
  CGSize constraint = _horizontal ? CGSizeMake(CGFLOAT_MAX, _windowCross) : CGSizeMake(_windowCross, CGFLOAT_MAX);
#if TARGET_OS_OSX
  CGSize fits = view.fittingSize;
  if ([view respondsToSelector:@selector(sizeThatFits:)]) fits = [(id)view sizeThatFits:constraint];
  if (!view.wantsLayer) view.wantsLayer = YES;
#else
  CGSize fits = [view sizeThatFits:constraint];
#endif
  CGFloat extent = _horizontal ? fits.width : fits.height;
  if (!(extent > 0)) extent = _horizontal ? view.frame.size.width : view.frame.size.height;
  double leading = _driver.stickyLeading(active, [self offset], extent);
  CGRect frame = _horizontal ? [self displayRect:leading y:0 width:extent height:_windowCross]
                            : [self displayRect:0 y:leading width:_windowCross height:extent];
  ShadowListKitPlace(view, frame);
  if (view.layer.zPosition != 2) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    view.layer.zPosition = 2;
    [CATransaction commit];
  }
  return YES;
}

- (void)unpinCell:(ShadowListKitListCell *)cell
{
  if (cell.row != NSNotFound && (std::size_t)cell.row < _driver.getRowCount()) {
    [self placeCell:cell frame:[self rowRect:(std::size_t)cell.row]];
  }
  if (cell != _heldCell) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    cell.layer.zPosition = 0;
    [CATransaction commit];
  }
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
  MountInputs inputs;
  inputs.windowAlong = _windowAlong;
  inputs.pad = _windowAlong * self.mountOverscan;
  inputs.leadingInset = [self leadingInset];
  inputs.trailingInset = [self trailingInset];
  double offset = [self offset];
  MountPlan plan = _driver.planMount(offset, inputs.windowAlong, inputs.pad, inputs.leadingInset, inputs.trailingInset);
  // A pass that covers what the last one did is skipped.
  MountRecord next{plan.low, plan.high, plan.sticky, _driver.getGeometryVersion(), _structureVersion,
    [self pixelShift], plan.band, inputs};
  BOOL skipped = shouldSkipMount(_mountRecord, next, offset, (std::size_t)self.numberOfColumns, [self hasHeldRow]);
  _mountRecord = next;
  if (skipped) {
    return;
  }
  [self mountPlan:plan];
  [self prefetchAround:plan];
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
  for (std::size_t index : _driver.getPersistentIndices()) {
    [self mountRow:index generation:generation];
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
  [_changes finishFades];
  for (auto& entry : _mounted) {
    [self recycleCell:entry.second];
  }
  _mounted.clear();
  _mountRecord = MountRecord();
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
  if (_atEndChanged) {
    _atEndChanged = NO;
    if ([_userDelegate respondsToSelector:@selector(listView:didChangeAtEnd:)]) {
      [_userDelegate listView:self didChangeAtEnd:_atEnd];
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
