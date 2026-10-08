#import "Internal/SLKListView+Private.h"
#import "Internal/SLKChangeAnimator.h"
#import "Internal/SLKListModels+Private.h"
#import "Internal/SLKSectionIndexView.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>

using namespace azimgd::shadowlist;

/*
 * Reuse identifier of the built-in section header and footer cells.
 */
static NSString *const SLK_SECTION_HEADER_IDENTIFIER = @"SLKSectionHeader";
static NSString *const SLK_SECTION_FOOTER_IDENTIFIER = @"SLKSectionFooter";

void SLKPlace(UIView *view, CGRect frame)
{
  CGRect bounds = CGRectMake(0, 0, frame.size.width, frame.size.height);
  if (!CGSizeEqualToSize(view.bounds.size, bounds.size)) {
    view.bounds = bounds;
  }
  CGPoint center = CGPointMake(CGRectGetMidX(frame), CGRectGetMidY(frame));
  if (!CGPointEqualToPoint(view.center, center)) {
    view.center = center;
  }
}

/*
 * The indices in a set, low to high.
 */
static std::vector<std::size_t> SLKIndices(NSIndexSet *set)
{
  std::vector<NSUInteger> raw(set.count);
  [set getIndexes:raw.data() maxCount:raw.size() inIndexRange:nil];
  return std::vector<std::size_t>(raw.begin(), raw.end());
}

static NSString *SLKString(const std::string& value)
{
  return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding] ?: @"";
}

@interface SLKListView ()
- (void)settleFrame;
@end

#pragma mark - Settle frame target

/*
 * The settle display link's target. It holds the list weakly, which lets the list go away
 * while the link exists.
 */
@interface SLKSettleTarget : NSObject
@property (nonatomic, weak) SLKListView *list;
@end

@implementation SLKSettleTarget

- (void)tick:(CADisplayLink *)link
{
  SLKListView *list = _list;
  if (!list) {
    [link invalidate];
    return;
  }
  [list settleFrame];
}

@end

#pragma mark - Section header cell

/*
 * The cell of a section header or footer the data source gives only a title for.
 */
@interface SLKSectionTitleCell : SLKListCell
@property (nonatomic, strong, readonly) UILabel *label;
@property (nonatomic) BOOL footer;
@end

@implementation SLKSectionTitleCell

- (instancetype)initWithReuseIdentifier:(NSString *)reuseIdentifier
{
  if (self = [super initWithReuseIdentifier:reuseIdentifier]) {
    _footer = [reuseIdentifier isEqualToString:SLK_SECTION_FOOTER_IDENTIFIER];
    _label = [UILabel new];
    _label.numberOfLines = 0;
    _label.textColor = UIColor.secondaryLabelColor;
    _label.font = _footer ? [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
                          : [UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];
    self.backgroundColor = _footer ? UIColor.clearColor : UIColor.secondarySystemBackgroundColor;
    [self addSubview:_label];
  }
  return self;
}

- (CGSize)sizeThatFits:(CGSize)size
{
  CGSize text = [_label sizeThatFits:CGSizeMake(MAX(0, size.width - 32), CGFLOAT_MAX)];
  return CGSizeMake(size.width, MAX(28, ceil(text.height) + 12));
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  _label.frame = CGRectMake(16, 6, MAX(0, self.bounds.size.width - 32), MAX(0, self.bounds.size.height - 12));
}

@end

#pragma mark - Delegate proxy

/*
 * Sits between UIScrollView and the user's delegate. The list sees the scroll events it needs
 * for snapping and gesture phases, and everything else goes to the user's delegate.
 */
@interface SLKDelegateProxy : NSProxy
@property (nonatomic, weak) SLKListView *list;
@property (nonatomic, weak) id<SLKListViewDelegate> target;
@end

@implementation SLKDelegateProxy

static BOOL SLKListHandles(SEL selector)
{
  return selector == @selector(scrollViewWillEndDragging:withVelocity:targetContentOffset:) ||
    selector == @selector(scrollViewDidEndDragging:willDecelerate:) ||
    selector == @selector(scrollViewDidEndDecelerating:) ||
    selector == @selector(scrollViewDidEndScrollingAnimation:) ||
    selector == @selector(scrollViewWillBeginDragging:);
}

- (BOOL)respondsToSelector:(SEL)selector
{
  return SLKListHandles(selector) || [_target respondsToSelector:selector];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector
{
  id target = SLKListHandles(selector) ? (id)_list : (id)_target;
  return [target methodSignatureForSelector:selector] ?: [NSObject instanceMethodSignatureForSelector:@selector(self)];
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL selector = invocation.selector;
  if (SLKListHandles(selector)) {
    [invocation invokeWithTarget:_list];
    if ([_target respondsToSelector:selector]) {
      [invocation invokeWithTarget:_target];
    }
  } else if ([_target respondsToSelector:selector]) {
    [invocation invokeWithTarget:_target];
  }
}

@end

#pragma mark - List

@implementation SLKListView {
  SLKDelegateProxy *_proxy;
  SLKChangeAnimator *_changes;
  id<SLKItemAnimator> _itemAnimator;

  NSUInteger _mountGeneration;

  // What the last mount pass covered. The pass is skipped while all of it holds.
  std::size_t _mountedLow;
  std::size_t _mountedHigh;
  std::size_t _mountedSticky;
  std::uint64_t _mountedGeometry;
  NSUInteger _mountedStructure;
  NSUInteger _structureVersion;

  NSMutableDictionary<NSString *, Class> *_cellClasses;
  NSMutableDictionary<NSString *, NSMutableArray<SLKListCell *> *> *_reusePool;

  SLKListCell *_stickyCell;

  // The band of offsets where the core has nothing to do.
  OffsetBand _band;
  BOOL _needsFrame;

  CGFloat _headerSize;
  CGFloat _footerSize;
  CGFloat _contentAlong;

  // The last offset seen, to tell the user's scrolling from our own writes.
  double _previousOffset;

  /*
   * The offset the core last asked for and the device pixel aligned content offset written
   * for it. While the scroll view rests there, the core is told the exact offset and rows are
   * drawn shifted by the difference, which keeps the content where the core holds it.
   */
  double _exactOffset;
  CGFloat _writtenAlong;
  BOOL _hasWrittenOffset;
  // The pixel shift the last mount pass placed rows with.
  double _mountedShift;
  BOOL _userScrolled;

  BOOL _inLayoutPass;
  // A settle frame waits for the next display frame, at most one at a time.
  BOOL _settleScheduled;
  CADisplayLink *_settleLink;
  // Whether the data source gives sizes and sections, read again on every reload.
  BOOL _sizesFromDataSource;
  BOOL _reachedStart;
  BOOL _reachedEnd;

  // Changes collected by performBatchUpdates:completion: until its block returns.
  NSInteger _batchDepth;
  BatchUpdate _batch;
  BOOL _batchNeedsReload;
  id _batchPayload;

  ListSelection _selection;
  __weak SLKListCell *_highlightedCell;

  // The content version of every item applyChanges saw, by key.
  ContentVersions _contentVersions;

  SLKAnchorState *_pendingAnchor;
  UIRefreshControl *_refresh;
  SLKSectionIndexView *_sectionIndex;

  std::vector<std::size_t> _prefetchRows;
  std::vector<std::size_t> _cancelRows;
}

@dynamic delegate;

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    [self commonInit];
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
  if (self = [super initWithCoder:coder]) {
    [self commonInit];
  }
  return self;
}

- (void)commonInit
{
  _proxy = [SLKDelegateProxy alloc];
  _proxy.list = self;
  [super setDelegate:(id<UIScrollViewDelegate>)_proxy];
  _cellClasses = [NSMutableDictionary new];
  _reusePool = [NSMutableDictionary new];
  _changes = [[SLKChangeAnimator alloc] initWithList:self];
  _itemAnimator = [SLKDefaultItemAnimator new];
  _numberOfColumns = 1;
  _estimatedItemSize = 120;
  _overscan = 1;
  _mountOverscan = 0.5;
  _startReachedThreshold = 1;
  _endReachedThreshold = 1;
  _mountedLow = UNDEFINED_INDEX;
  _mountedHigh = UNDEFINED_INDEX;
  _mountedSticky = UNDEFINED_INDEX;
  _needsFrame = YES;
  _allowsSelection = YES;
  _separatorColor = UIColor.separatorColor;
  _separatorInsetStart = 16;

  __weak SLKListView *weakSelf = self;
  _driver.setMeasureItem([weakSelf](std::size_t index, const std::string& key, double cross) -> double {
    SLKListView *list = weakSelf;
    return list ? [list measureRow:(NSInteger)index key:key cross:(CGFloat)cross] : 0.0;
  });

  UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap:)];
  tap.cancelsTouchesInView = NO;
  [self addGestureRecognizer:tap];
  [self installActionGestures];
}

- (void)dealloc
{
  [_settleLink invalidate];
}

#pragma mark - Delegate

- (void)setDataSource:(id<SLKListViewDataSource>)dataSource
{
  _dataSource = dataSource;
  _sizesFromDataSource = [dataSource respondsToSelector:@selector(listView:sizeForItemAtIndex:crossSize:)];
}

- (void)setDelegate:(id<SLKListViewDelegate>)delegate
{
  _userDelegate = delegate;
  _proxy.target = delegate;
  // UIScrollView caches which methods its delegate answers. Set it again so it asks anew.
  [super setDelegate:nil];
  [super setDelegate:(id<UIScrollViewDelegate>)_proxy];
  [self installActionGestures];
}

- (id<SLKListViewDelegate>)delegate
{
  return _userDelegate;
}

#pragma mark - Axis

- (CGFloat)along:(CGPoint)point
{
  return _horizontal ? point.x : point.y;
}

- (CGFloat)cross:(CGPoint)point
{
  return _horizontal ? point.y : point.x;
}

- (CGFloat)leadingInset
{
  UIEdgeInsets inset = self.adjustedContentInset;
  return _horizontal ? inset.left : inset.top;
}

- (CGFloat)trailingInset
{
  UIEdgeInsets inset = self.adjustedContentInset;
  return _horizontal ? inset.right : inset.bottom;
}

- (double)offset
{
  CGFloat along = [self along:self.contentOffset];
  if ([self restsOnWrittenOffset:along]) {
    return _exactOffset;
  }
  return along + [self leadingInset];
}

- (BOOL)restsOnWrittenOffset:(CGFloat)along
{
  return _hasWrittenOffset && std::fabs(along - _writtenAlong) < 1e-3;
}

/*
 * How far the core's offset is past the content offset the scroll view shows, under one
 * device pixel. Rows are drawn this much earlier.
 */
- (double)pixelShift
{
  CGFloat along = [self along:self.contentOffset];
  return [self restsOnWrittenOffset:along] ? _exactOffset - (along + [self leadingInset]) : 0.0;
}

- (CGFloat)displayScale
{
  CGFloat scale = self.traitCollection.displayScale;
  return scale >= 1 ? scale : UIScreen.mainScreen.scale;
}

- (CGFloat)pixelAligned:(double)value
{
  CGFloat scale = [self displayScale];
  return (CGFloat)(std::round(value * scale) / scale);
}

- (CGFloat)maxOffset
{
  return MAX(0, _contentAlong - _windowAlong);
}

/*
 * The scroll view only rests on device pixels. The content offset goes there rounded, and
 * the exact offset is kept for the core while the view stays put.
 */
- (void)writeOffset:(double)offset byUser:(BOOL)byUser
{
  _previousOffset = offset;
  if (byUser) {
    _userScrolled = YES;
  }
  CGPoint point = [self contentOffsetAt:offset];
  CGFloat along = [self pixelAligned:[self along:point]];
  if (_horizontal) {
    point.x = along;
  } else {
    point.y = along;
  }
  _exactOffset = offset;
  _writtenAlong = along;
  _hasWrittenOffset = YES;
  self.contentOffset = point;
}

/*
 * The content offset that puts the list at an offset along the axis.
 */
- (CGPoint)contentOffsetAt:(double)offset
{
  CGPoint point = self.contentOffset;
  CGFloat value = (CGFloat)offset - [self leadingInset];
  if (_horizontal) {
    point.x = value;
  } else {
    point.y = value;
  }
  return point;
}

- (ScrollPhase)scrollPhase
{
  if (self.isTracking || self.isDragging) {
    return ScrollPhase::Dragging;
  }
  return self.isDecelerating ? ScrollPhase::Settling : ScrollPhase::Idle;
}

#pragma mark - Properties

- (void)setInverted:(BOOL)inverted
{
  _inverted = inverted;
  [self invalidateFrame];
}

- (void)setFollowAppends:(BOOL)followAppends
{
  _followAppends = followAppends;
  [self invalidateFrame];
}

- (void)setHorizontal:(BOOL)horizontal
{
  _horizontal = horizontal;
  [self invalidateFrame];
}

- (void)setNumberOfColumns:(NSInteger)numberOfColumns
{
  _numberOfColumns = MAX(1, numberOfColumns);
  [self invalidateFrame];
}

- (void)setSnapToItem:(BOOL)snapToItem
{
  _snapToItem = snapToItem;
  [self invalidateFrame];
}

- (void)setReorderEnabled:(BOOL)reorderEnabled
{
  _reorderEnabled = reorderEnabled;
  [self enableDragPress:reorderEnabled];
}

- (void)setStickyIndices:(NSIndexSet *)stickyIndices
{
  _stickyIndices = [stickyIndices copy];
  [self updateStickyRows];
  [self invalidateFrame];
}

- (void)setStickySectionHeaders:(BOOL)stickySectionHeaders
{
  _stickySectionHeaders = stickySectionHeaders;
  [self updateStickyRows];
  [self invalidateFrame];
}

- (void)setHeaderView:(UIView *)headerView
{
  [_headerView removeFromSuperview];
  _headerView = headerView;
  if (headerView) {
    [self insertSubview:headerView atIndex:0];
  }
  [self invalidateFrame];
}

- (void)setFooterView:(UIView *)footerView
{
  [_footerView removeFromSuperview];
  _footerView = footerView;
  if (footerView) {
    [self insertSubview:footerView atIndex:0];
  }
  [self invalidateFrame];
}

- (id<SLKItemAnimator>)itemAnimator
{
  return _itemAnimator;
}

- (void)setItemAnimator:(id<SLKItemAnimator>)itemAnimator
{
  _itemAnimator = itemAnimator ?: [SLKDefaultItemAnimator new];
}

- (void)setAllowsSelection:(BOOL)allowsSelection
{
  _allowsSelection = allowsSelection;
  if (!allowsSelection) {
    [self clearSelection];
  }
}

- (void)setAllowsMultipleSelection:(BOOL)allowsMultipleSelection
{
  _allowsMultipleSelection = allowsMultipleSelection;
  _selection.setMultiple(allowsMultipleSelection);
  if (!allowsMultipleSelection && _selection.getCount() > 1) {
    [self clearSelection];
  }
}

- (void)setEditing:(BOOL)editing
{
  [self setEditing:editing animated:NO];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated
{
  _editing = editing;
  [self closeSwipeAnimated:animated];
  for (auto& entry : _mounted) {
    if (entry.second.isEditing != editing) {
      [entry.second setEditing:editing animated:animated];
    }
  }
}

- (BOOL)isEditing
{
  return _editing;
}

- (void)closeSwipeActionsAnimated:(BOOL)animated
{
  [self closeSwipeAnimated:animated];
}

- (void)setRefreshEnabled:(BOOL)refreshEnabled
{
  _refreshEnabled = refreshEnabled;
  if (refreshEnabled && !_refresh) {
    _refresh = [UIRefreshControl new];
    [_refresh addTarget:self action:@selector(refreshPulled) forControlEvents:UIControlEventValueChanged];
  }
  self.refreshControl = refreshEnabled ? _refresh : nil;
}

- (BOOL)isRefreshing
{
  return _refresh.isRefreshing;
}

- (void)setRefreshing:(BOOL)refreshing
{
  if (refreshing == _refresh.isRefreshing) {
    return;
  }
  if (refreshing) {
    [_refresh beginRefreshing];
  } else {
    [_refresh endRefreshing];
  }
}

- (void)refreshPulled
{
  if ([_userDelegate respondsToSelector:@selector(listViewDidBeginRefreshing:)]) {
    [_userDelegate listViewDidBeginRefreshing:self];
  }
}

- (void)setShowsSeparators:(BOOL)showsSeparators
{
  _showsSeparators = showsSeparators;
  [self structureChanged];
}

- (void)setSeparatorColor:(UIColor *)separatorColor
{
  _separatorColor = separatorColor ?: UIColor.separatorColor;
  [self structureChanged];
}

- (void)setSeparatorInsetStart:(CGFloat)separatorInsetStart
{
  _separatorInsetStart = separatorInsetStart;
  [self structureChanged];
}

- (void)setSeparatorInsetEnd:(CGFloat)separatorInsetEnd
{
  _separatorInsetEnd = separatorInsetEnd;
  [self structureChanged];
}

- (void)setSeparatorThickness:(CGFloat)separatorThickness
{
  _separatorThickness = separatorThickness;
  [self structureChanged];
}

/*
 * Run the core on the next layout pass even inside the band.
 */
- (void)invalidateFrame
{
  _needsFrame = YES;
  [self setNeedsLayout];
}

- (ListSettings)listSettings
{
  ListSettings settings;
  settings.estimatedItemSize = _estimatedItemSize;
  settings.overscan = _overscan;
  settings.startReachedThreshold = _startReachedThreshold;
  settings.endReachedThreshold = _endReachedThreshold;
  settings.columns = (std::size_t)_numberOfColumns;
  settings.inverted = _inverted;
  settings.followAppends = _followAppends;
  settings.horizontal = _horizontal;
  settings.snapToItem = _snapToItem;
  settings.snapAlignment = (int)_snapAlignment;
  return settings;
}

#pragma mark - Cells

- (void)registerClass:(Class)cellClass forCellReuseIdentifier:(NSString *)identifier
{
  _cellClasses[identifier] = cellClass;
}

- (__kindof SLKListCell *)dequeueReusableCellWithIdentifier:(NSString *)identifier
{
  NSMutableArray<SLKListCell *> *pool = _reusePool[identifier];
  SLKListCell *cell = pool.lastObject;
  if (cell) {
    [pool removeLastObject];
    [cell prepareForReuse];
    return cell;
  }
  Class cellClass = _cellClasses[identifier] ?: [SLKListCell class];
  cell = [[cellClass alloc] initWithReuseIdentifier:identifier];
  cell.hidden = YES;
  [self addSubview:cell];
  return cell;
}

- (void)recycleCell:(SLKListCell *)cell
{
  NSInteger index = cell.index;
  [self swipeCellWillRecycle:cell];
  [cell.layer removeAllAnimations];
  cell.alpha = 1;
  cell.transform = CGAffineTransformIdentity;
  cell.hidden = YES;
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
  NSMutableArray<SLKListCell *> *pool = _reusePool[cell.reuseIdentifier];
  if (!pool) {
    pool = [NSMutableArray new];
    _reusePool[cell.reuseIdentifier] = pool;
  }
  [pool addObject:cell];
}

/*
 * The cell of a row: an item's from the data source, or a section header or footer.
 */
- (SLKListCell *)makeCellAtRow:(NSInteger)row
{
  RowPlace place = _sections.placeOfRow((std::size_t)row);
  SLKListCell *cell = nil;
  switch (place.kind) {
    case RowKind::Item:
      cell = [_dataSource listView:self cellForItemAtIndex:(NSInteger)place.item];
      break;
    case RowKind::Header:
    case RowKind::Footer:
      cell = [self sectionCellAtSection:(NSInteger)place.section footer:place.kind == RowKind::Footer];
      break;
  }
  if (cell.superview != self) {
    [self addSubview:cell];
  }
  cell.row = row;
  cell.index = place.item == UNDEFINED_INDEX ? NSNotFound : (NSInteger)place.item;
  return cell;
}

- (SLKListCell *)sectionCellAtSection:(NSInteger)section footer:(BOOL)footer
{
  if (!footer && [_dataSource respondsToSelector:@selector(listView:cellForHeaderInSection:)]) {
    return [_dataSource listView:self cellForHeaderInSection:section];
  }
  if (footer && [_dataSource respondsToSelector:@selector(listView:cellForFooterInSection:)]) {
    return [_dataSource listView:self cellForFooterInSection:section];
  }
  NSString *identifier = footer ? SLK_SECTION_FOOTER_IDENTIFIER : SLK_SECTION_HEADER_IDENTIFIER;
  if (!_cellClasses[identifier]) {
    _cellClasses[identifier] = [SLKSectionTitleCell class];
  }
  SLKSectionTitleCell *cell = [self dequeueReusableCellWithIdentifier:identifier];
  cell.label.text = footer ? [_dataSource listView:self titleForFooterInSection:section]
                           : [_dataSource listView:self titleForHeaderInSection:section];
  [cell setNeedsLayout];
  return cell;
}

/*
 * The mounted cell of a row, or nil.
 */
- (SLKListCell *)mountedCellAtIndex:(std::size_t)index
{
  if (index >= _driver.getKeyCount()) {
    return nil;
  }
  auto mounted = _mounted.find(_driver.getKeyAt(index));
  return mounted != _mounted.end() ? mounted->second : nil;
}

- (SLKListCell *)mountedCellForKey:(const std::string&)key
{
  auto mounted = _mounted.find(key);
  return mounted != _mounted.end() ? mounted->second : nil;
}

#pragma mark - Data

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
      keys.emplace_back([_dataSource listView:self keyForItemAtIndex:index].UTF8String);
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
      itemKeys.emplace_back([_dataSource listView:self keyForItemAtIndex:(NSInteger)(first + local)].UTF8String);
    }
    if (sectionKeys) {
      keysOfSections[section] = std::string([_dataSource listView:self keyForSection:section].UTF8String);
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
    std::vector<std::size_t> inserted = SLKIndices(indices);
    _batch.inserted.insert(_batch.inserted.end(), inserted.begin(), inserted.end());
    return;
  }
  if ([self hasSections]) {
    [self reloadData];
    return;
  }
  // An index past the end inserts at the end. The key is read where the row lands.
  std::vector<std::size_t> inserted = insertionPositions(SLKIndices(indices), _driver.getKeyCount());
  std::vector<std::string> keys;
  keys.reserve(inserted.size());
  for (std::size_t index : inserted) {
    keys.emplace_back([_dataSource listView:self keyForItemAtIndex:(NSInteger)index].UTF8String);
  }
  if (_animatesChanges) {
    [_changes captureRemoved:{} inserted:keys];
  }
  _driver.insertKeys(std::move(inserted), std::move(keys));
  _sections.setPlain(_driver.getKeyCount());
  [self structureChanged];
}

- (void)deleteItemsAtIndices:(NSIndexSet *)indices
{
  if (indices.count == 0) {
    return;
  }
  if (_batchDepth > 0) {
    std::vector<std::size_t> deleted = SLKIndices(indices);
    _batch.deleted.insert(_batch.deleted.end(), deleted.begin(), deleted.end());
    return;
  }
  if ([self hasSections]) {
    [self reloadData];
    return;
  }
  std::vector<std::size_t> deleted = deletionPositions(SLKIndices(indices), _driver.getKeyCount());
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
}

- (void)reloadItemsAtIndices:(NSIndexSet *)indices
{
  [self reloadItemsAtIndices:indices payload:nil];
}

- (void)reloadItemsAtIndices:(NSIndexSet *)indices payload:(id)payload
{
  if (_batchDepth > 0) {
    std::vector<std::size_t> reloaded = SLKIndices(indices);
    _batch.reloaded.insert(_batch.reloaded.end(), reloaded.begin(), reloaded.end());
    _batchPayload = payload;
    return;
  }
  std::vector<std::size_t> rows;
  for (std::size_t item : SLKIndices(indices)) {
    std::size_t row = _sections.rowForItem(item);
    if (row != UNDEFINED_INDEX) {
      rows.push_back(row);
    }
  }
  [self reloadRows:rows payload:payload];
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
    SLKListCell *cell = [self mountedCellAtIndex:row];
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
  if (_animatesChanges && self.window && [_itemAnimator isKindOfClass:[SLKDefaultItemAnimator class]]) {
    wait = ((SLKDefaultItemAnimator *)_itemAnimator).duration;
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
        return std::string([_dataSource listView:self keyForItemAtIndex:(NSInteger)index].UTF8String);
      });
      _sections.setPlain(next.size());
      planned = YES;
    } else {
      NSLog(@"[SLKListView] batch updates do not add up to %ld items, reloading", (long)nextCount);
    }
  }
  if (!planned) {
    next = [self readRowKeys];
  }
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
  if (!planned) {
    [self reloadSectionIndex];
  }
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

- (SLKListChanges *)applyChanges
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

  if (_animatesChanges) {
    [self captureChangeTo:next];
  }
  _driver.reloadKeys(std::move(next));
  std::vector<std::size_t> rows = rowsOfKeys(_driver.getKeys(), reloadedKeys);
  if (rows.empty()) {
    [self structureChanged];
  } else {
    [self reloadRows:rows payload:nil];
  }
  [self reloadSectionIndex];

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
  return [[SLKListChanges alloc] initWithDeleted:deleted inserted:inserted movedFrom:movedFrom movedTo:movedTo
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

#pragma mark - Sections

- (NSInteger)itemForRow:(NSInteger)row
{
  if (row < 0) {
    return NSNotFound;
  }
  std::size_t item = _sections.itemForRow((std::size_t)row);
  return item == UNDEFINED_INDEX ? NSNotFound : (NSInteger)item;
}

- (NSInteger)rowForItem:(NSInteger)item
{
  if (item < 0) {
    return NSNotFound;
  }
  std::size_t row = _sections.rowForItem((std::size_t)item);
  return row == UNDEFINED_INDEX ? NSNotFound : (NSInteger)row;
}

- (NSInteger)numberOfSections
{
  return _sections.isSectioned() ? (NSInteger)_sections.getSectionCount() : 1;
}

- (NSInteger)sectionForItemAtIndex:(NSInteger)index
{
  if (index < 0) {
    return NSNotFound;
  }
  std::size_t section = _sections.sectionForItem((std::size_t)index);
  return section == UNDEFINED_INDEX ? NSNotFound : (NSInteger)section;
}

- (NSInteger)firstItemIndexInSection:(NSInteger)section
{
  if (section < 0) {
    return NSNotFound;
  }
  std::size_t item = _sections.firstItemInSection((std::size_t)section);
  return item == UNDEFINED_INDEX ? NSNotFound : (NSInteger)item;
}

- (CGRect)rectForHeaderInSection:(NSInteger)section
{
  std::size_t row = section < 0 ? UNDEFINED_INDEX : _sections.headerRow((std::size_t)section);
  if (row == UNDEFINED_INDEX || row >= _driver.getCount()) {
    return CGRectNull;
  }
  return [self rowRect:row];
}

- (void)scrollToSection:(NSInteger)section animated:(BOOL)animated
{
  std::size_t row = section < 0 ? UNDEFINED_INDEX : _sections.firstRowInSection((std::size_t)section);
  if (row == UNDEFINED_INDEX) {
    return;
  }
  [self scrollToRow:row viewPosition:0 animated:animated];
}

/*
 * Map the drop of a held row to item indices. In a list with sections a row stays in its own
 * section and a drop outside goes to the nearest end of it.
 */
- (BOOL)itemsForDragFromRow:(std::size_t)fromRow toRow:(std::size_t)toRow from:(NSInteger *)from to:(NSInteger *)to
{
  std::size_t fromItem = _sections.itemForRow(fromRow);
  std::size_t toItem = _sections.itemForDrop(fromRow, toRow);
  if (fromItem == UNDEFINED_INDEX || toItem == UNDEFINED_INDEX) {
    return NO;
  }
  *from = (NSInteger)fromItem;
  *to = (NSInteger)toItem;
  return fromItem != toItem;
}

- (void)updateStickyRows
{
  std::vector<std::size_t> items = _stickyIndices.count > 0 ? SLKIndices(_stickyIndices) : std::vector<std::size_t>();
  _driver.setStickyIndices(_sections.stickyRows(items, _stickySectionHeaders));
}

/*
 * The section index along the trailing edge, from the data source's titles. Vertical lists only.
 */
- (void)reloadSectionIndex
{
  NSArray<NSString *> *titles = nil;
  if (!_horizontal && [_dataSource respondsToSelector:@selector(sectionIndexTitlesForListView:)]) {
    titles = [_dataSource sectionIndexTitlesForListView:self];
  }
  if (titles.count == 0) {
    [_sectionIndex removeFromSuperview];
    _sectionIndex = nil;
    return;
  }
  if (!_sectionIndex) {
    _sectionIndex = [SLKSectionIndexView new];
    _sectionIndex.layer.zPosition = 10;
    __weak SLKListView *weakSelf = self;
    _sectionIndex.onSelect = ^(NSInteger index) {
      [weakSelf scrollToSectionIndexTitleAtIndex:index];
    };
    [self addSubview:_sectionIndex];
  }
  _sectionIndex.titles = titles;
  [self setNeedsLayout];
}

- (void)scrollToSectionIndexTitleAtIndex:(NSInteger)index
{
  NSInteger section = index;
  if ([_dataSource respondsToSelector:@selector(listView:sectionForSectionIndexTitle:atIndex:)]) {
    section = [_dataSource listView:self sectionForSectionIndexTitle:_sectionIndex.titles[index] atIndex:index];
  }
  [self scrollToSection:section animated:NO];
}

/*
 * Keep the section index on the visible trailing edge, inside the insets.
 */
- (void)layoutSectionIndex
{
  if (!_sectionIndex) {
    return;
  }
  CGRect visible = UIEdgeInsetsInsetRect(self.bounds, self.adjustedContentInset);
  CGFloat width = [_sectionIndex sizeThatFits:visible.size].width;
  CGRect frame = CGRectMake(CGRectGetMaxX(visible) - width, CGRectGetMinY(visible), width, visible.size.height);
  if (!CGRectEqualToRect(_sectionIndex.frame, frame)) {
    _sectionIndex.frame = frame;
    [_sectionIndex setNeedsDisplay];
  }
  [self bringSubviewToFront:_sectionIndex];
}

- (BOOL)touchesShouldCancelInContentView:(UIView *)view
{
  if (_sectionIndex && [view isDescendantOfView:_sectionIndex]) {
    return NO;
  }
  return [super touchesShouldCancelInContentView:view];
}

#pragma mark - Layout

- (void)layoutSubviews
{
  [super layoutSubviews];
  [self layoutPass];
}

/*
 * One pass runs the core when the offset left the band, then mounts. Everything lands
 * before the frame is drawn.
 */
- (void)layoutPass
{
  if (_inLayoutPass) {
    return;
  }
  _inLayoutPass = YES;
  BOOL geometryChanged = [self readGeometry];
  [self trackUserScroll];
  if (_windowCross > 0 && _windowAlong > 0) {
    if (_needsFrame || geometryChanged || !_band.contains([self offset])) {
      [self runPasses];
    }
    [self layoutTemplates];
    [self mountCells];
    [self layoutSticky];
    if ([self hasHeldRow]) {
      [self applyDragShiftsAnimated:NO];
    }
    [_changes run];
    [self layoutSwipe];
    [self layoutSectionIndex];
  }
  _inLayoutPass = NO;
  [self dispatchReached];
}

/*
 * Read the viewport and template sizes. Returns whether any changed since the last pass.
 */
- (BOOL)readGeometry
{
  CGSize bounds = self.bounds.size;
  UIEdgeInsets inset = self.adjustedContentInset;
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
  if ([self scrollPhase] != ScrollPhase::Idle) {
    _userScrolled = YES;
  }
  _previousOffset = current;
}

- (CGFloat)templateSize:(UIView *)view cross:(CGFloat)cross
{
  if (!view) {
    return 0;
  }
  CGSize size = view.frame.size;
  CGFloat along = _horizontal ? size.width : size.height;
  if (along > 0) {
    return along;
  }
  CGSize fits = [view sizeThatFits:_horizontal ? CGSizeMake(CGFLOAT_MAX, cross) : CGSizeMake(cross, CGFLOAT_MAX)];
  return _horizontal ? fits.width : fits.height;
}

- (void)layoutTemplates
{
  if (_headerView) {
    CGRect frame = _horizontal ? [self displayRect:0 y:0 width:_headerSize height:_windowCross]
                               : [self displayRect:0 y:0 width:_windowCross height:_headerSize];
    if (!CGRectEqualToRect(_headerView.frame, frame)) {
      _headerView.frame = frame;
    }
  }
  if (_footerView) {
    double start = _driver.getFooterStart(_footerSize);
    CGRect frame = _horizontal ? [self displayRect:start y:0 width:_footerSize height:_windowCross]
                               : [self displayRect:0 y:start width:_windowCross height:_footerSize];
    if (!CGRectEqualToRect(_footerView.frame, frame)) {
      _footerView.frame = frame;
    }
  }
}

/*
 * Run the core until the window is measured and any correction landed. Everything happens
 * before the frame is drawn. The reader never sees an estimate or a correction.
 */
- (void)runPasses
{
  _driver.setSettings([self listSettings]);
  PassInput input;
  input.offset = [self offset];
  input.windowAlong = _windowAlong;
  input.windowCross = _windowCross;
  input.headerSize = _headerSize;
  input.footerSize = _footerSize;
  input.phase = [self scrollPhase];
  input.userScrolled = _userScrolled;
  input.tracking = self.isTracking;
  PassResult result = _driver.runPasses(input);
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
  if (std::fabs(result.offset - [self offset]) >= 0.01) {
    [self writeOffset:result.offset byUser:NO];
  }
  _band = result.band;
  _reachedStart = _reachedStart || result.reachedStart;
  _reachedEnd = _reachedEnd || result.reachedEnd;
  if (result.settling) {
    [self scheduleSettleFrame];
  }
}

- (void)applyContentSize:(CGFloat)along
{
  if (along == _contentAlong && !CGSizeEqualToSize(self.contentSize, CGSizeZero)) {
    return;
  }
  _contentAlong = along;
  self.contentSize = _horizontal ? CGSizeMake(along, _windowCross) : CGSizeMake(_windowCross, along);
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
    SLKSettleTarget *target = [SLKSettleTarget new];
    target.list = self;
    _settleLink = [CADisplayLink displayLinkWithTarget:target selector:@selector(tick:)];
    [_settleLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
  }
  _settleLink.paused = NO;
}

- (void)settleFrame
{
  _settleLink.paused = YES;
  _settleScheduled = NO;
  [self setNeedsLayout];
  [self layoutIfNeeded];
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
  CGFloat left = [self pixelAligned:x - shiftX];
  CGFloat top = [self pixelAligned:y - shiftY];
  CGFloat right = [self pixelAligned:x + width - shiftX];
  CGFloat bottom = [self pixelAligned:y + height - shiftY];
  return CGRectMake(left, top, right - left, bottom - top);
}

/*
 * Mount the cells of rows near the viewport and recycle the rest. Rows are matched by key,
 * which keeps the content of a cell that only moved. Rows under translucent bars show and
 * are mounted too.
 */
- (void)mountCells
{
  if (!_driver.getMeasuredWindow()) {
    [self unmountAll];
    return;
  }
  MountPlan plan = _driver.planMount([self offset], _windowAlong, _windowAlong * _mountOverscan, [self leadingInset],
    [self trailingInset]);
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
  NSUInteger generation = ++_mountGeneration;
  for (std::size_t index = plan.low; plan.low != UNDEFINED_INDEX && index <= plan.high; ++index) {
    if (_driver.shouldMount(plan, index)) {
      [self mountRow:index generation:generation];
    }
  }
  if (plan.sticky != UNDEFINED_INDEX) {
    [self mountRow:plan.sticky generation:generation];
  }
  // The held row stays mounted wherever the finger takes it.
  std::size_t held = _driver.getHeldIndex();
  if (held != UNDEFINED_INDEX) {
    [self mountRow:held generation:generation];
  }
  [self recycleStaleCells:generation];
}

- (void)mountRow:(std::size_t)index generation:(NSUInteger)generation
{
  if (index >= _driver.getCount() || index >= _driver.getKeyCount()) {
    return;
  }
  const std::string& key = _driver.getKeyAt(index);
  SLKListCell *cell = nil;
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
  SLKPlace(cell, [self rowRect:index]);
  [self placeSeparatorOfCell:cell row:index];
  if (!appearing) {
    return;
  }
  cell.hidden = NO;
  [self applyStateToCell:cell key:key];
  if (cell.index != NSNotFound && [_userDelegate respondsToSelector:@selector(listView:willDisplayCell:atIndex:)]) {
    [_userDelegate listView:self willDisplayCell:cell atIndex:cell.index];
  }
}

/*
 * A cell coming on screen shows its row's selection and the list's editing state.
 */
- (void)applyStateToCell:(SLKListCell *)cell key:(const std::string&)key
{
  BOOL selected = cell.index != NSNotFound && _selection.contains(key);
  if (cell.isSelected != selected) {
    [cell setSelected:selected animated:NO];
  }
  if (cell.isEditing != _editing) {
    [cell setEditing:_editing animated:NO];
  }
}

- (void)recycleStaleCells:(NSUInteger)generation
{
  for (auto entry = _mounted.begin(); entry != _mounted.end();) {
    if (entry->second.mountGeneration != generation) {
      // A removed row's cell fades out first, then goes back to the pool. One swiped out does not.
      if ([self isSwipedOutCell:entry->second] || ![_changes fadeOutKey:entry->first cell:entry->second]) {
        [self recycleCell:entry->second];
      }
      entry = _mounted.erase(entry);
    } else {
      ++entry;
    }
  }
}

- (void)unmountAll
{
  for (auto& entry : _mounted) {
    [self recycleCell:entry.second];
  }
  _mounted.clear();
  _mountedLow = UNDEFINED_INDEX;
  _mountedHigh = UNDEFINED_INDEX;
  _mountedSticky = UNDEFINED_INDEX;
}

#pragma mark - Prefetching

/*
 * Tell the prefetch data source about items the measured window brought in that have no cell
 * yet, and the ones that left it unseen.
 */
- (void)prefetchAround:(const MountPlan&)plan
{
  id<SLKListViewPrefetchDataSource> prefetch = _prefetchDataSource;
  if (!prefetch) {
    return;
  }
  _driver.updatePrefetch(plan.low, plan.high, _prefetchRows, _cancelRows);
  NSIndexSet *items = [self itemsOfRows:_prefetchRows];
  if (items.count > 0) {
    [prefetch listView:self prefetchItemsAtIndices:items];
  }
  NSIndexSet *cancelled = [self itemsOfRows:_cancelRows];
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
 * Called by the core for every row in its window that has no size yet. Reads the size from
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
  SLKListCell *cell = nil;
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

#pragma mark - Sticky

/*
 * Pin the active section header and put the one it replaced back in its row.
 */
- (void)layoutSticky
{
  if (!_driver.hasSticky()) {
    return;
  }
  double offset = [self offset];
  std::size_t active = _driver.activeStickyIndex(offset);
  SLKListCell *cell = active != UNDEFINED_INDEX ? [self mountedCellAtIndex:active] : nil;
  if (_stickyCell && _stickyCell != cell) {
    [self unpinCell:_stickyCell];
  }
  _stickyCell = cell;
  if (!cell) {
    return;
  }
  RowRect rect = _driver.getRowRect(active);
  double pinned = _driver.stickyLeading(active, offset);
  if (_horizontal) {
    rect.x = pinned;
  } else {
    rect.y = pinned;
  }
  SLKPlace(cell, [self displayRect:rect.x y:rect.y width:rect.width height:rect.height]);
  cell.layer.zPosition = 1;
}

- (void)unpinCell:(SLKListCell *)cell
{
  if (cell.row != NSNotFound && (std::size_t)cell.row < _driver.getCount()) {
    SLKPlace(cell, [self rowRect:(std::size_t)cell.row]);
  }
  cell.layer.zPosition = 0;
}

#pragma mark - Separators

/*
 * The line over an item's trailing edge when another item of its section follows. Grids have
 * none.
 */
- (void)placeSeparatorOfCell:(SLKListCell *)cell row:(std::size_t)row
{
  BOOL shows = _showsSeparators && _numberOfColumns == 1 && row + 1 < _driver.getKeyCount() &&
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
  CGFloat thickness = _separatorThickness > 0 ? _separatorThickness : 1 / [self displayScale];
  CGSize size = cell.bounds.size;
  CGRect frame = _horizontal
    ? CGRectMake(size.width - thickness, _separatorInsetStart, thickness,
        MAX(0, size.height - _separatorInsetStart - _separatorInsetEnd))
    : CGRectMake(_separatorInsetStart, size.height - thickness,
        MAX(0, size.width - _separatorInsetStart - _separatorInsetEnd), thickness);
  line.hidden = NO;
  line.frame = frame;
  line.backgroundColor = [_separatorColor resolvedColorWithTraitCollection:self.traitCollection].CGColor;
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

/*
 * The visible cell under a point in the list's own coordinates.
 */
- (SLKListCell *)cellAtPoint:(CGPoint)point
{
  for (auto& entry : _mounted) {
    SLKListCell *cell = entry.second;
    if (!cell.hidden && cell.row != NSNotFound && CGRectContainsPoint(cell.frame, point)) {
      return cell;
    }
  }
  return nil;
}

- (SLKListCell *)itemCellAtPoint:(CGPoint)point
{
  SLKListCell *cell = [self cellAtPoint:point];
  return cell.index != NSNotFound ? cell : nil;
}

- (void)handleTap:(UITapGestureRecognizer *)tap
{
  CGPoint point = [tap locationInView:self];
  if ([self swipeOwnsTapAtPoint:point] || [self hasHeldRow]) {
    return;
  }
  SLKListCell *cell = [self itemCellAtPoint:point];
  if (cell && _allowsSelection) {
    [self userSelectedCell:cell];
  }
}

#pragma mark - Selection

/*
 * A tap on a row. Multiple selection toggles it, single selection moves to it.
 */
- (void)userSelectedCell:(SLKListCell *)cell
{
  NSInteger index = cell.index;
  const std::string& key = _driver.getKeyAt((std::size_t)cell.row);
  if (_allowsMultipleSelection && _selection.contains(key)) {
    _selection.deselect(key);
    [cell setSelected:NO animated:YES];
    if ([_userDelegate respondsToSelector:@selector(listView:didDeselectItemAtIndex:)]) {
      [_userDelegate listView:self didDeselectItemAtIndex:index];
    }
    return;
  }
  if ([_userDelegate respondsToSelector:@selector(listView:shouldSelectItemAtIndex:)] &&
      ![_userDelegate listView:self shouldSelectItemAtIndex:index]) {
    return;
  }
  std::vector<std::string> deselected;
  _selection.select(key, deselected);
  for (const std::string& previous : deselected) {
    SLKListCell *previousCell = [self mountedCellForKey:previous];
    [previousCell setSelected:NO animated:YES];
    NSInteger previousIndex = [self itemIndexOfKey:previous];
    if (previousIndex != NSNotFound && [_userDelegate respondsToSelector:@selector(listView:didDeselectItemAtIndex:)]) {
      [_userDelegate listView:self didDeselectItemAtIndex:previousIndex];
    }
  }
  if (!cell.isSelected) {
    [cell setSelected:YES animated:YES];
  }
  if ([_userDelegate respondsToSelector:@selector(listView:didSelectItemAtIndex:)]) {
    [_userDelegate listView:self didSelectItemAtIndex:index];
  }
}

- (NSInteger)itemIndexOfKey:(const std::string&)key
{
  const std::vector<std::string>& keys = _driver.getKeys();
  auto found = std::find(keys.begin(), keys.end(), key);
  return found == keys.end() ? NSNotFound : [self itemForRow:(NSInteger)(found - keys.begin())];
}

- (NSIndexSet *)selectedIndices
{
  return [self itemsOfRows:_selection.indicesIn(_driver.getKeys())];
}

- (void)selectItemAtIndex:(NSInteger)index animated:(BOOL)animated
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound || !_allowsSelection) {
    return;
  }
  const std::string& key = _driver.getKeyAt((std::size_t)row);
  std::vector<std::string> deselected;
  _selection.select(key, deselected);
  for (const std::string& previous : deselected) {
    [[self mountedCellForKey:previous] setSelected:NO animated:animated];
  }
  SLKListCell *cell = [self mountedCellForKey:key];
  if (cell && !cell.isSelected) {
    [cell setSelected:YES animated:animated];
  }
}

- (void)deselectItemAtIndex:(NSInteger)index animated:(BOOL)animated
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound) {
    return;
  }
  const std::string& key = _driver.getKeyAt((std::size_t)row);
  if (_selection.deselect(key)) {
    [[self mountedCellForKey:key] setSelected:NO animated:animated];
  }
}

- (void)clearSelection
{
  std::vector<std::string> deselected;
  _selection.clear(deselected);
  for (const std::string& key : deselected) {
    [[self mountedCellForKey:key] setSelected:NO animated:NO];
  }
}

/*
 * A finger resting on a selectable row highlights it. UIScrollView delays the touch until it
 * knows the finger is not scrolling, and cancels it when it scrolls after all.
 */
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesBegan:touches withEvent:event];
  if (!_allowsSelection || touches.count != 1 || [self hasHeldRow] || [self isSwipeOpen]) {
    return;
  }
  SLKListCell *cell = [self itemCellAtPoint:[touches.anyObject locationInView:self]];
  if (!cell || ([_userDelegate respondsToSelector:@selector(listView:shouldHighlightItemAtIndex:)] &&
                ![_userDelegate listView:self shouldHighlightItemAtIndex:cell.index])) {
    return;
  }
  [cell setHighlighted:YES animated:NO];
  _highlightedCell = cell;
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesEnded:touches withEvent:event];
  [self unhighlight];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesCancelled:touches withEvent:event];
  [self unhighlight];
}

- (void)unhighlight
{
  SLKListCell *cell = _highlightedCell;
  _highlightedCell = nil;
  if (cell.isHighlighted) {
    [cell setHighlighted:NO animated:YES];
  }
}

#pragma mark - Scroll events

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gesture
{
  if (gesture == _swipePan) {
    return [self shouldBeginActionGesture:gesture];
  }
  if (gesture == self.panGestureRecognizer) {
    CGPoint start = [gesture locationInView:self];
    if (_sectionIndex && CGRectContainsPoint(_sectionIndex.frame, start)) {
      return NO;
    }
    if ([self swipeTakesPan:self.panGestureRecognizer]) {
      return NO;
    }
  }
  return [super gestureRecognizerShouldBegin:gesture];
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  _driver.cancelLanding();
  [self unhighlight];
  [self closeSwipeAnimated:YES];
}

- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint *)targetContentOffset
{
  if (!_snapToItem) {
    return;
  }
  double target = [self along:*targetContentOffset] + [self leadingInset];
  double snapped = _driver.nearestSnapOffset(target) - [self leadingInset];
  if (_horizontal) {
    targetContentOffset->x = (CGFloat)snapped;
  } else {
    targetContentOffset->y = (CGFloat)snapped;
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
  if (!decelerate) {
    // The gesture ended. The core needs an idle frame to let a held correction go.
    [self invalidateFrame];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
  [self invalidateFrame];
}

/*
 * An animated command ended. The core lands exactly on its target.
 */
- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
  if (_driver.land()) {
    [self invalidateFrame];
  }
}

#pragma mark - Queries

- (SLKListCell *)cellForItemAtIndex:(NSInteger)index
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound) {
    return nil;
  }
  SLKListCell *cell = [self mountedCellAtIndex:(std::size_t)row];
  return cell.hidden ? nil : cell;
}

- (NSArray<SLKListCell *> *)visibleCells
{
  NSMutableArray<SLKListCell *> *cells = [NSMutableArray arrayWithCapacity:_mounted.size()];
  CGRect visible = self.bounds;
  for (auto& entry : _mounted) {
    SLKListCell *cell = entry.second;
    if (!cell.hidden && cell.index != NSNotFound && CGRectIntersectsRect(cell.frame, visible)) {
      [cells addObject:cell];
    }
  }
  [cells sortUsingComparator:^NSComparisonResult(SLKListCell *a, SLKListCell *b) {
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
  if (row == NSNotFound || (std::size_t)row >= _driver.getCount()) {
    return CGRectNull;
  }
  return [self rowRect:(std::size_t)row];
}

#pragma mark - Saved position

- (SLKAnchorState *)anchorState
{
  std::optional<ListAnchor> anchor = _driver.getAnchor([self offset]);
  if (!anchor) {
    return _pendingAnchor;
  }
  return [[SLKAnchorState alloc] initWithKey:SLKString(anchor->key) offset:(CGFloat)anchor->offset];
}

- (void)restoreAnchorState:(SLKAnchorState *)state
{
  _pendingAnchor = state;
  [self restorePendingAnchor];
}

/*
 * Land the saved row once the data has it. Until then the position waits.
 */
- (void)restorePendingAnchor
{
  if (!_pendingAnchor) {
    return;
  }
  ListAnchor anchor{_pendingAnchor.key.UTF8String, (double)_pendingAnchor.offset};
  if (!_driver.restoreAnchor(anchor)) {
    return;
  }
  _pendingAnchor = nil;
  [self stopScrolling];
  [self invalidateFrame];
}

- (void)encodeRestorableStateWithCoder:(NSCoder *)coder
{
  [super encodeRestorableStateWithCoder:coder];
  SLKAnchorState *state = self.anchorState;
  if (state) {
    [coder encodeObject:state forKey:@"SLKAnchorState"];
  }
}

- (void)decodeRestorableStateWithCoder:(NSCoder *)coder
{
  [super decodeRestorableStateWithCoder:coder];
  SLKAnchorState *state = [coder decodeObjectOfClass:[SLKAnchorState class] forKey:@"SLKAnchorState"];
  if (state) {
    [self restoreAnchorState:state];
  }
}

#pragma mark - Accessibility

/*
 * VoiceOver walks every row, not only the mounted ones: the header, then each row, then the
 * footer. A row it moves to that is not mounted is scrolled into view first, which mounts it.
 */
- (NSInteger)accessibilityElementCount
{
  return (NSInteger)_driver.getKeyCount() + (_headerView ? 1 : 0) + (_footerView ? 1 : 0);
}

- (id)accessibilityElementAtIndex:(NSInteger)index
{
  NSInteger rows = (NSInteger)_driver.getKeyCount();
  NSInteger row = index - (_headerView ? 1 : 0);
  if (row < 0) {
    return index == 0 ? _headerView : nil;
  }
  if (row >= rows) {
    return row == rows ? _footerView : nil;
  }
  SLKListCell *cell = [self mountedCellAtIndex:(std::size_t)row];
  if (!cell || cell.hidden || ![self isRowOnScreen:(std::size_t)row]) {
    [self scrollToRow:(std::size_t)row viewPosition:0 animated:NO];
    [self layoutIfNeeded];
    cell = [self mountedCellAtIndex:(std::size_t)row];
  }
  return cell;
}

- (NSInteger)indexOfAccessibilityElement:(id)element
{
  if (element && element == _headerView) {
    return 0;
  }
  if (element && element == _footerView) {
    return [self accessibilityElementCount] - 1;
  }
  UIView *view = [element isKindOfClass:[UIView class]] ? element : nil;
  while (view && ![view isKindOfClass:[SLKListCell class]]) {
    view = view.superview;
  }
  SLKListCell *cell = (SLKListCell *)view;
  if (!cell || cell.superview != self || cell.row == NSNotFound) {
    return NSNotFound;
  }
  return cell.row + (_headerView ? 1 : 0);
}

- (BOOL)isRowOnScreen:(std::size_t)index
{
  CGRect rect = [self rowRect:index];
  CGRect visible = UIEdgeInsetsInsetRect(self.bounds, self.adjustedContentInset);
  return CGRectIntersectsRect(rect, visible);
}

/*
 * A three finger swipe moves one viewport and says which rows show.
 */
- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction
{
  BOOL forward = _horizontal
    ? (direction == UIAccessibilityScrollDirectionLeft || direction == UIAccessibilityScrollDirectionNext)
    : (direction == UIAccessibilityScrollDirectionUp || direction == UIAccessibilityScrollDirectionNext);
  BOOL backward = _horizontal
    ? (direction == UIAccessibilityScrollDirectionRight || direction == UIAccessibilityScrollDirectionPrevious)
    : (direction == UIAccessibilityScrollDirectionDown || direction == UIAccessibilityScrollDirectionPrevious);
  if (!forward && !backward) {
    return NO;
  }
  double offset = [self offset];
  double target = pageScrollTarget(offset, _windowAlong, [self maxOffset], forward ? 1 : -1);
  if (std::fabs(target - offset) < 1) {
    return NO;
  }
  [self stopScrolling];
  [self writeOffset:target byUser:YES];
  [self layoutIfNeeded];
  NSRange visible = self.visibleRange;
  NSString *status = [NSString stringWithFormat:@"Rows %lu to %lu of %lu", (unsigned long)visible.location + 1,
    (unsigned long)NSMaxRange(visible), (unsigned long)_sections.getItemCount()];
  UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification, status);
  return YES;
}

#pragma mark - Scroll commands

- (void)scrollToItemAtIndex:(NSInteger)index viewPosition:(CGFloat)viewPosition animated:(BOOL)animated
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound) {
    return;
  }
  [self scrollToRow:(std::size_t)row viewPosition:viewPosition animated:animated];
}

- (void)scrollToRow:(std::size_t)row viewPosition:(CGFloat)viewPosition animated:(BOOL)animated
{
  if (row >= _driver.getKeyCount()) {
    return;
  }
  if (animated && row < _driver.getCount()) {
    // Animate to the estimate, then let the core land exactly when the animation ends.
    double target = _driver.animatedTargetOffset(row, viewPosition, _windowAlong, [self maxOffset]);
    [self animateCommandTo:target landing:{ScrollLanding::Target::Index, row, viewPosition}];
    return;
  }
  [self stopScrolling];
  _driver.scrollToIndex(row, viewPosition);
  [self runCommandNow];
}

- (void)scrollToStartAnimated:(BOOL)animated
{
  if (animated) {
    // The header shows at the very start, which landing on row 0 would scroll past.
    [self animateCommandTo:0 landing:{ScrollLanding::Target::Start, 0, 0}];
    return;
  }
  [self stopScrolling];
  _driver.scrollToStart();
  [self runCommandNow];
}

- (void)scrollToEndAnimated:(BOOL)animated
{
  if (animated) {
    [self animateCommandTo:[self maxOffset] landing:{ScrollLanding::Target::End, 0, 0}];
    return;
  }
  [self stopScrolling];
  _driver.scrollToEnd();
  [self runCommandNow];
}

- (void)animateCommandTo:(double)target landing:(ScrollLanding)landing
{
  _driver.setLanding(landing);
  [self setContentOffset:[self contentOffsetAt:target] animated:YES];
}

/*
 * Stop momentum and any animated command still on its way. Otherwise they keep writing
 * the offset over a command that runs now.
 */
- (void)stopScrolling
{
  [self setContentOffset:self.contentOffset animated:NO];
  _driver.cancelLanding();
}

- (void)runCommandNow
{
  [self invalidateFrame];
  [self layoutIfNeeded];
}

@end
