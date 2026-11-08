#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitChangeAnimator.h"
#import "Internal/ShadowListKitListModels+Private.h"
#import "Internal/ShadowListKitListSupport.h"
#import "Internal/ShadowListKitSectionIndexView.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * Keeps every category file linked into apps that link the kit as a static library without -ObjC.
 */
__attribute__((used)) static const char *const ShadowListKitListViewCategoryLinks[] = {
  &ShadowListKitListViewAccessibilityLink,
  &ShadowListKitListViewActionsLink,
  &ShadowListKitListViewCommandsLink,
  &ShadowListKitListViewDataLink,
  &ShadowListKitListViewDragLink,
  &ShadowListKitListViewPassesLink,
  &ShadowListKitListViewSectionsLink,
  &ShadowListKitListViewSelectionLink,
};

void ShadowListKitPlace(UIView *view, CGRect frame)
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
 * What a layout pass reads for every row, read once after the core ran: the offset, the pixel
 * shift, the display scale and the insets. Valid until the pass ends. A content offset or inset
 * that changes during the pass reads them again.
 */
struct ShadowListKitPassGeometry {
  double offset = 0.0;
  double shift = 0.0;
  CGFloat scale = 1.0;
  CGFloat leadingInset = 0.0;
  CGFloat trailingInset = 0.0;
  bool valid = false;
};

#pragma mark - List

// The categories in ShadowListKitListView+*.mm implement the rest of the class interface.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wincomplete-implementation"
@implementation ShadowListKitListView {
  ShadowListKitDelegateProxy *_proxy;
  ShadowListKitPassGeometry _passGeometry;

  /*
   * The offset the core last asked for and the device pixel aligned content offset written
   * for it. While the scroll view rests there, the core is told the exact offset and rows are
   * drawn shifted by the difference, which keeps the content where the core holds it. The
   * leading inset of that moment counts too. A refresh control that ends changes the inset
   * without moving the content offset, and the exact offset written before is then wrong by the change.
   */
  double _exactOffset;
  CGFloat _writtenAlong;
  CGFloat _writtenInset;
  BOOL _hasWrittenOffset;

  BOOL _inLayoutPass;

  UIRefreshControl *_refresh;
}
#pragma clang diagnostic pop

@dynamic delegate;
@dynamic numberOfSections;
@dynamic selectedIndices;
@dynamic anchorState;

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
  _proxy = [ShadowListKitDelegateProxy alloc];
  _proxy.list = self;
  [super setDelegate:(id<UIScrollViewDelegate>)_proxy];
  _cellClasses = [NSMutableDictionary new];
  _reusePool = [NSMutableDictionary new];
  _changes = [[ShadowListKitChangeAnimator alloc] initWithList:self];
  _itemAnimator = [ShadowListKitDefaultItemAnimator new];
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
  // Like UITableView. A list shorter than the viewport still gets the safe area inset and bounces.
  self.alwaysBounceVertical = YES;

  __weak ShadowListKitListView *weakSelf = self;
  _driver.setMeasureRow([weakSelf](std::size_t row, const std::string& key, double cross) -> double {
    ShadowListKitListView *list = weakSelf;
    return list ? [list measureRow:(NSInteger)row key:key cross:(CGFloat)cross] : 0.0;
  });
  _driver.setPrefetchRows([weakSelf](const std::vector<std::size_t>& prefetch, const std::vector<std::size_t>& cancel) {
    [weakSelf reportPrefetchRows:prefetch cancel:cancel];
  });

  UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap:)];
  tap.cancelsTouchesInView = NO;
  [self addGestureRecognizer:tap];
  [self installActionGestures];
}

- (void)dealloc
{
  [_settleLink invalidate];
  [_dragLink invalidate];
}

#pragma mark - Delegate

- (void)setDataSource:(id<ShadowListKitListViewDataSource>)dataSource
{
  _dataSource = dataSource;
  _sizesFromDataSource = [dataSource respondsToSelector:@selector(listView:sizeForItemAtIndex:crossSize:)];
}

- (void)setDelegate:(id<ShadowListKitListViewDelegate>)delegate
{
  _userDelegate = delegate;
  _proxy.target = delegate;
  // UIScrollView caches which methods its delegate answers. Set it again so it asks anew.
  [super setDelegate:nil];
  [super setDelegate:(id<UIScrollViewDelegate>)_proxy];
  [self installActionGestures];
}

- (id<ShadowListKitListViewDelegate>)delegate
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
  if (_passGeometry.valid) {
    return _passGeometry.leadingInset;
  }
  UIEdgeInsets inset = self.adjustedContentInset;
  return _horizontal ? inset.left : inset.top;
}

- (CGFloat)trailingInset
{
  if (_passGeometry.valid) {
    return _passGeometry.trailingInset;
  }
  UIEdgeInsets inset = self.adjustedContentInset;
  return _horizontal ? inset.right : inset.bottom;
}

- (double)offset
{
  if (_passGeometry.valid) {
    return _passGeometry.offset;
  }
  CGFloat along = [self along:self.contentOffset];
  if ([self restsOnWrittenOffset:along]) {
    return _exactOffset;
  }
  return along + [self leadingInset];
}

- (BOOL)restsOnWrittenOffset:(CGFloat)along
{
  return _hasWrittenOffset && std::fabs(along - _writtenAlong) < 1e-3 &&
    std::fabs([self leadingInset] - _writtenInset) < 1e-3;
}

/*
 * How far the core's offset is past the content offset the scroll view shows, under one
 * device pixel. Rows are drawn this much earlier.
 */
- (double)pixelShift
{
  if (_passGeometry.valid) {
    return _passGeometry.shift;
  }
  CGFloat along = [self along:self.contentOffset];
  return [self restsOnWrittenOffset:along] ? _exactOffset - (along + [self leadingInset]) : 0.0;
}

- (CGFloat)displayScale
{
  if (_passGeometry.valid) {
    return _passGeometry.scale;
  }
  CGFloat scale = self.traitCollection.displayScale;
  return scale >= 1 ? scale : UIScreen.mainScreen.scale;
}

- (void)readPassGeometry
{
  _passGeometry.valid = false;
  CGFloat leadingInset = [self leadingInset];
  CGFloat trailingInset = [self trailingInset];
  double offset = [self offset];
  double shift = [self pixelShift];
  CGFloat scale = [self displayScale];
  _passGeometry.offset = offset;
  _passGeometry.shift = shift;
  _passGeometry.scale = scale;
  _passGeometry.leadingInset = leadingInset;
  _passGeometry.trailingInset = trailingInset;
  _passGeometry.valid = true;
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
  _writtenInset = [self leadingInset];
  _hasWrittenOffset = YES;
  self.contentOffset = point;
}

/*
 * A content offset set during a layout pass, by the pass itself or by a delegate it called, makes
 * the pass read its geometry again.
 */
- (void)setContentOffset:(CGPoint)contentOffset
{
  [super setContentOffset:contentOffset];
  if (_passGeometry.valid) {
    [self readPassGeometry];
  }
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

/*
 * What moves the offset now, in the core's words.
 */
- (ScrollPhase)corePhase
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
  if (horizontal != _horizontal) {
    self.alwaysBounceVertical = !horizontal;
    self.alwaysBounceHorizontal = horizontal;
  }
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

- (id<ShadowListKitItemAnimator>)itemAnimator
{
  return _itemAnimator;
}

- (void)setItemAnimator:(id<ShadowListKitItemAnimator>)itemAnimator
{
  _itemAnimator = itemAnimator ?: [ShadowListKitDefaultItemAnimator new];
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

#pragma mark - Cells

/*
 * The cells sit in the scroll view itself.
 */
- (UIView *)cellContainer
{
  return self;
}

/*
 * A recycled cell loses its swipe, its animations and its drag look.
 */
- (void)clearCellForReuse:(ShadowListKitListCell *)cell
{
  [self swipeCellWillRecycle:cell];
  [cell.layer removeAllAnimations];
  cell.alpha = 1;
  cell.transform = CGAffineTransformIdentity;
}

- (void)placeCell:(ShadowListKitListCell *)cell frame:(CGRect)frame
{
  ShadowListKitPlace(cell, frame);
}

- (void)setCell:(ShadowListKitListCell *)cell hidden:(BOOL)hidden
{
  cell.hidden = hidden;
}

#pragma mark - Layout

- (void)layoutSubviews
{
  _inLayout = YES;
  [super layoutSubviews];
  [self layoutPass];
  _inLayout = NO;
}

/*
 * Safe area, bar and keyboard insets change the viewport without a layout pass of their own.
 */
- (void)adjustedContentInsetDidChange
{
  [super adjustedContentInsetDidChange];
  if (_passGeometry.valid) {
    [self readPassGeometry];
  }
  [self invalidateFrame];
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
  ++_layoutPasses;
  BOOL geometryChanged = [self readGeometry];
  [self trackUserScroll];
  if (_windowCross > 0 && _windowAlong > 0) {
    if (_needsFrame || geometryChanged || !_band.contains([self offset])) {
      [self runPasses];
    }
    [self readPassGeometry];
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
  _passGeometry.valid = false;
  _inLayoutPass = NO;
  [self dispatchReached];
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
    CGRect frame = _horizontal ? [self displayRect:_footerStart y:0 width:_footerSize height:_windowCross]
                               : [self displayRect:0 y:_footerStart width:_windowCross height:_footerSize];
    if (!CGRectEqualToRect(_footerView.frame, frame)) {
      _footerView.frame = frame;
    }
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
  ShadowListKitListCell *cell = active != UNDEFINED_INDEX ? [self mountedCellAtIndex:active] : nil;
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
  ShadowListKitPlace(cell, [self displayRect:rect.x y:rect.y width:rect.width height:rect.height]);
  cell.layer.zPosition = 1;
}

- (void)unpinCell:(ShadowListKitListCell *)cell
{
  if (cell.row != NSNotFound && (std::size_t)cell.row < _driver.getRowCount()) {
    ShadowListKitPlace(cell, [self rowRect:(std::size_t)cell.row]);
  }
  cell.layer.zPosition = 0;
}

#pragma mark - Separators

/*
 * The separator color as the list's traits show it.
 */
- (CGColorRef)separatorCGColor
{
  return [self.separatorColor resolvedColorWithTraitCollection:self.traitCollection].CGColor;
}

#pragma mark - Events

/*
 * The visible cell under a point in the list's own coordinates.
 */
- (ShadowListKitListCell *)cellAtPoint:(CGPoint)point
{
  for (auto& entry : _mounted) {
    ShadowListKitListCell *cell = entry.second;
    if (!cell.hidden && cell.row != NSNotFound && CGRectContainsPoint(cell.frame, point)) {
      return cell;
    }
  }
  return nil;
}

- (ShadowListKitListCell *)itemCellAtPoint:(CGPoint)point
{
  ShadowListKitListCell *cell = [self cellAtPoint:point];
  return cell.index != NSNotFound ? cell : nil;
}

- (void)handleTap:(UITapGestureRecognizer *)tap
{
  CGPoint point = [tap locationInView:self];
  if ([self swipeOwnsTapAtPoint:point] || [self hasHeldRow]) {
    return;
  }
  ShadowListKitListCell *cell = [self itemCellAtPoint:point];
  if (cell && _allowsSelection) {
    [self userSelectedCell:cell];
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

@end
