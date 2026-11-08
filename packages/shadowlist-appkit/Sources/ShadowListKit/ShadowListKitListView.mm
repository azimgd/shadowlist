#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitChangeAnimator.h"
#import "Internal/ShadowListKitListModels+Private.h"
#import "Internal/ShadowListKitListSupport.h"

#import <QuartzCore/QuartzCore.h>

#include <algorithm>
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
  &ShadowListKitListViewScrollingLink,
  &ShadowListKitListViewSectionsLink,
  &ShadowListKitListViewSelectionLink,
};

void ShadowListKitPlace(NSView *view, CGRect frame)
{
  if (NSEqualRects(view.frame, frame)) {
    return;
  }
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  view.frame = frame;
  [CATransaction commit];
}

/*
 * What a layout pass reads for every row, read once after the core ran: the offset, the pixel
 * shift, the backing scale and the insets. Valid until the pass ends. A content offset or inset
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
  ShadowListKitPassGeometry _passGeometry;

  /*
   * The content offset along the axis, exact. The content view shows it on device pixels and
   * rows are drawn shifted by the difference, which keeps the content where the core holds it.
   */
  double _offsetAlong;

  BOOL _inLayoutPass;

  /*
   * The separator color as the list's appearance shows it, resolved on first use.
   */
  CGColorRef _separatorCGColor;
}
#pragma clang diagnostic pop

@dynamic numberOfSections;
@dynamic anchorState;
@dynamic selectedIndices;
@dynamic clickedIndex;
@dynamic reorderEnabled;

- (instancetype)initWithFrame:(NSRect)frame
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
  self.wantsLayer = YES;
  self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;
  self.clipsToBounds = YES;
  _contentView = [[ShadowListKitContentView alloc] initWithFrame:self.bounds];
  [self addSubview:_contentView];
  _cellClasses = [NSMutableDictionary new];
  _reusePool = [NSMutableDictionary new];
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
  _scrollEnabled = YES;
  _separatorColor = NSColor.separatorColor;
  _separatorInsetStart = 16;

  __weak ShadowListKitListView *weakSelf = self;
  _driver.setMeasureRow([weakSelf](std::size_t row, const std::string& key, double cross) -> double {
    ShadowListKitListView *list = weakSelf;
    return list ? [list measureRow:(NSInteger)row key:key cross:(CGFloat)cross] : 0.0;
  });
  _driver.setPrefetchRows([weakSelf](const std::vector<std::size_t>& prefetch, const std::vector<std::size_t>& cancel) {
    [weakSelf reportPrefetchRows:prefetch cancel:cancel];
  });
  [self setUpScrolling];
}

- (void)dealloc
{
  [_settleLink invalidate];
  [_dragLink invalidate];
  [_swipeLink invalidate];
  if (_dragKeyMonitor) {
    [NSEvent removeMonitor:_dragKeyMonitor];
  }
  if (_swipeKeyMonitor) {
    [NSEvent removeMonitor:_swipeKeyMonitor];
  }
  CGColorRelease(_separatorCGColor);
}

- (BOOL)isFlipped
{
  return YES;
}

- (void)setDataSource:(id<ShadowListKitListViewDataSource>)dataSource
{
  _dataSource = dataSource;
  _sizesFromDataSource = [dataSource respondsToSelector:@selector(listView:sizeForItemAtIndex:crossSize:)];
}

- (id<ShadowListKitListViewDataSource>)dataSource
{
  return _dataSource;
}

- (void)setDelegate:(id<ShadowListKitListViewDelegate>)delegate
{
  _userDelegate = delegate;
  [self updateSwipesItems];
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

- (NSEdgeInsets)adjustedContentInset
{
  NSEdgeInsets safe = self.safeAreaInsets;
  return NSEdgeInsetsMake(_contentInset.top + safe.top, _contentInset.left + safe.left,
    _contentInset.bottom + safe.bottom, _contentInset.right + safe.right);
}

- (CGFloat)leadingInset
{
  if (_passGeometry.valid) {
    return _passGeometry.leadingInset;
  }
  NSEdgeInsets inset = self.adjustedContentInset;
  return _horizontal ? inset.left : inset.top;
}

- (CGFloat)trailingInset
{
  if (_passGeometry.valid) {
    return _passGeometry.trailingInset;
  }
  NSEdgeInsets inset = self.adjustedContentInset;
  return _horizontal ? inset.right : inset.bottom;
}

- (double)offset
{
  if (_passGeometry.valid) {
    return _passGeometry.offset;
  }
  return _offsetAlong + [self leadingInset];
}

/*
 * How far the exact offset is past the device pixel the content view shows, under one device
 * pixel. Rows are drawn this much earlier.
 */
- (double)pixelShift
{
  if (_passGeometry.valid) {
    return _passGeometry.shift;
  }
  return _offsetAlong - [self pixelAligned:_offsetAlong];
}

- (CGFloat)displayScale
{
  if (_passGeometry.valid) {
    return _passGeometry.scale;
  }
  CGFloat scale = self.window.backingScaleFactor;
  if (scale < 1) {
    scale = NSScreen.mainScreen.backingScaleFactor;
  }
  return scale >= 1 ? scale : 1;
}

- (void)readPassGeometry
{
  _passGeometry.valid = false;
  CGFloat leadingInset = [self leadingInset];
  CGFloat trailingInset = [self trailingInset];
  double offset = [self offset];
  CGFloat scale = [self displayScale];
  double shift = [self pixelShift];
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
 * Move to an offset the list asked for. A driven list asks its host, which sets contentOffset.
 */
- (void)writeOffset:(double)offset byUser:(BOOL)byUser
{
  _previousOffset = offset;
  if (byUser) {
    _userScrolled = YES;
  }
  CGPoint point = [self contentOffsetAt:offset];
  if (!_scrollDriven) {
    self.contentOffset = point;
    return;
  }
  if ([_userDelegate respondsToSelector:@selector(listView:didCorrectOffset:)]) {
    [_userDelegate listView:self didCorrectOffset:point];
  }
}

- (CGPoint)contentOffset
{
  NSEdgeInsets inset = self.adjustedContentInset;
  return _horizontal ? CGPointMake((CGFloat)_offsetAlong, -inset.top) : CGPointMake(-inset.left, (CGFloat)_offsetAlong);
}

/*
 * A content offset set during a layout pass, by the pass itself or by a delegate it called, makes
 * the pass read its geometry again.
 */
- (void)setContentOffset:(CGPoint)contentOffset
{
  double along = [self along:contentOffset];
  if (along == _offsetAlong) {
    return;
  }
  _offsetAlong = along;
  [self placeContentView];
  if (_passGeometry.valid) {
    [self readPassGeometry];
  }
  [self scrollGeometryChanged];
  if (!_inLayoutPass) {
    self.needsLayout = YES;
  }
  if ([_userDelegate respondsToSelector:@selector(listViewDidScroll:)]) {
    [_userDelegate listViewDidScroll:self];
  }
}

/*
 * An animated offset eases there and listViewDidEndScrollingAnimation: follows when it arrives. A
 * driven list's offset lands at once.
 */
- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated
{
  if (animated && !_scrollDriven) {
    [self animateToOffset:[self along:contentOffset] + [self leadingInset] landing:ScrollLanding{}];
    return;
  }
  [self stopScrolling];
  self.contentOffset = contentOffset;
  if (animated && [_userDelegate respondsToSelector:@selector(listViewDidEndScrollingAnimation:)]) {
    [_userDelegate listViewDidEndScrollingAnimation:self];
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

- (CGSize)contentSize
{
  return _horizontal ? CGSizeMake(_contentAlong, _windowCross) : CGSizeMake(_windowCross, _contentAlong);
}

- (void)setContentInset:(NSEdgeInsets)contentInset
{
  _contentInset = contentInset;
  [self placeContentView];
  if (_passGeometry.valid) {
    [self readPassGeometry];
  }
  [self invalidateFrame];
}

/*
 * The content view covers the list. Its bounds origin is the content offset on device pixels.
 */
- (void)placeContentView
{
  NSRect frame = self.bounds;
  CGPoint offset = self.contentOffset;
  CGFloat along = [self pixelAligned:[self along:offset]];
  NSPoint origin = _horizontal ? NSMakePoint(along, offset.y) : NSMakePoint(offset.x, along);
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  if (!NSEqualRects(_contentView.frame, frame)) {
    _contentView.frame = frame;
  }
  if (!NSEqualPoints(_contentView.bounds.origin, origin)) {
    [_contentView setBoundsOrigin:origin];
  }
  [CATransaction commit];
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
  _engine.axes = horizontal ? ShadowListKitScrollAxesHorizontal : ShadowListKitScrollAxesVertical;
  _scrollers.axes = _engine.axes;
  [self invalidateFrame];
}

- (void)setNumberOfColumns:(NSInteger)numberOfColumns
{
  _numberOfColumns = MAX(1, numberOfColumns);
  [self invalidateFrame];
}

- (void)setEstimatedItemSize:(CGFloat)estimatedItemSize
{
  _estimatedItemSize = estimatedItemSize;
  [self invalidateFrame];
}

- (void)setSnapToItem:(BOOL)snapToItem
{
  _snapToItem = snapToItem;
  [self invalidateFrame];
}

- (void)setSnapAlignment:(ShadowListKitSnapAlignment)snapAlignment
{
  _snapAlignment = snapAlignment;
  [self invalidateFrame];
}

- (NSIndexSet *)stickyIndices
{
  return _stickyIndices;
}

- (void)setStickyIndices:(NSIndexSet *)stickyIndices
{
  _stickyIndices = [stickyIndices copy];
  [self updateStickyRows];
  [self invalidateFrame];
}

- (BOOL)stickySectionHeaders
{
  return _stickySectionHeaders;
}

- (void)setStickySectionHeaders:(BOOL)stickySectionHeaders
{
  _stickySectionHeaders = stickySectionHeaders;
  [self updateStickyRows];
  [self invalidateFrame];
}

- (NSView *)headerView
{
  return _headerView;
}

/*
 * The header and footer views sit in the content under every row.
 */
- (void)setHeaderView:(NSView *)headerView
{
  [_headerView removeFromSuperview];
  _headerView = headerView;
  if (headerView) {
    [_contentView addSubview:headerView positioned:NSWindowBelow relativeTo:nil];
  }
  [self invalidateFrame];
}

- (NSView *)footerView
{
  return _footerView;
}

- (void)setFooterView:(NSView *)footerView
{
  [_footerView removeFromSuperview];
  _footerView = footerView;
  if (footerView) {
    [_contentView addSubview:footerView positioned:NSWindowBelow relativeTo:nil];
  }
  [self invalidateFrame];
}

- (BOOL)animatesChanges
{
  return _animatesChanges;
}

/*
 * The change animator exists while animations are on. Cells fading out finish on their own.
 */
- (void)setAnimatesChanges:(BOOL)animatesChanges
{
  _animatesChanges = animatesChanges;
  if (animatesChanges && !_changes) {
    _changes = [[ShadowListKitChangeAnimator alloc] initWithList:self];
  } else if (!animatesChanges) {
    _changes = nil;
  }
}

- (id<ShadowListKitItemAnimator>)itemAnimator
{
  return _itemAnimator;
}

- (void)setItemAnimator:(id<ShadowListKitItemAnimator>)itemAnimator
{
  _itemAnimator = itemAnimator ?: [ShadowListKitDefaultItemAnimator new];
}

/*
 * Without selection the list takes no first responder and hovers nothing.
 */
- (void)setAllowsSelection:(BOOL)allowsSelection
{
  if (allowsSelection == _allowsSelection) {
    return;
  }
  _allowsSelection = allowsSelection;
  if (!allowsSelection) {
    [self clearSelection];
    [self unhighlight];
    _hoverActive = NO;
    [self updateHover];
    if (self.window.firstResponder == self) {
      [self.window makeFirstResponder:nil];
    }
  }
  [self updateTrackingAreas];
  [self updateEmphasis];
}

- (void)setAllowsMultipleSelection:(BOOL)allowsMultipleSelection
{
  _allowsMultipleSelection = allowsMultipleSelection;
  _selection.setMultiple(allowsMultipleSelection);
  if (!allowsMultipleSelection && _selection.getCount() > 1) {
    [self clearSelection];
  }
}

- (void)setShowsSeparators:(BOOL)showsSeparators
{
  _showsSeparators = showsSeparators;
  [self structureChanged];
}

- (void)setSeparatorColor:(NSColor *)separatorColor
{
  _separatorColor = separatorColor ?: NSColor.separatorColor;
  [self dropResolvedColors];
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

- (void)setScrollDriven:(BOOL)scrollDriven
{
  if (scrollDriven == _scrollDriven) {
    return;
  }
  [self stopScrolling];
  _scrollDriven = scrollDriven;
  self.scrollPhase = ShadowListKitScrollPhaseIdle;
  [self scrollGeometryChanged];
}

#pragma mark - Cells

/*
 * The cells sit in the content view, which scrolls by its bounds origin.
 */
- (NSView *)cellContainer
{
  return _contentView;
}

/*
 * A recycled cell loses its animations, its pinned slot, hover, menu target, swipe and drag look.
 */
- (void)clearCellForReuse:(ShadowListKitListCell *)cell
{
  [cell.layer removeAllAnimations];
  cell.alphaValue = 1;
  if (cell == _stickyCell) {
    _stickyCell = nil;
  }
  if (cell.isHovered) {
    [cell setHovered:NO animated:NO];
  }
  if (cell.isMenuTarget) {
    [self endMenuTarget];
  }
  [self swipeCellWillRecycle:cell];
  [self resetDragLookOfCell:cell];
}

#pragma mark - Layout

- (void)layout
{
  _inLayout = YES;
  [super layout];
  [self layoutPass];
  _inLayout = NO;
}

- (void)setFrameSize:(NSSize)newSize
{
  [super setFrameSize:newSize];
  self.needsLayout = YES;
  [self noteFocusRingMaskChanged];
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
  [self placeContentView];
  [self trackUserScroll];
  if (_windowCross > 0 && _windowAlong > 0) {
    if (_needsFrame || geometryChanged || !_band.contains([self offset])) {
      [self runPasses];
      if ([self measuredRangeEndsShort]) {
        [self runPasses];
      }
    }
    [self readPassGeometry];
    [self layoutTemplates];
    [self mountCells];
    [self layoutSticky];
    if ([self hasHeldRow]) {
      [self updateDrag];
    }
    [_changes run];
    [self layoutSwipe];
  }
  _passGeometry.valid = false;
  _inLayoutPass = NO;
  [self scrollGeometryChanged];
  // The content may have moved under a still pointer.
  if (_hoverActive || _hoveredCell) {
    [self updateHover];
  }
  [self dispatchReached];
  if ([_userDelegate respondsToSelector:@selector(listViewDidLayOut:)]) {
    [_userDelegate listViewDidLayOut:self];
  }
}

/*
 * A header or footer view's size along the axis: its frame's, or what it fits while that is empty.
 */
- (CGFloat)templateSize:(NSView *)view cross:(CGFloat)cross
{
  if (!view) {
    return 0;
  }
  NSSize size = view.frame.size;
  CGFloat along = _horizontal ? size.width : size.height;
  if (along > 0) {
    return along;
  }
  NSSize fits = view.fittingSize;
  if ([view respondsToSelector:@selector(sizeThatFits:)]) {
    fits = [(id)view sizeThatFits:_horizontal ? NSMakeSize(CGFLOAT_MAX, cross) : NSMakeSize(cross, CGFLOAT_MAX)];
  }
  return _horizontal ? fits.width : fits.height;
}

- (void)layoutTemplates
{
  if (_headerView) {
    CGRect frame = _horizontal ? [self displayRect:0 y:0 width:_headerSize height:_windowCross]
                               : [self displayRect:0 y:0 width:_windowCross height:_headerSize];
    ShadowListKitPlace(_headerView, frame);
  }
  if (_footerView) {
    CGRect frame = _horizontal ? [self displayRect:_footerStart y:0 width:_footerSize height:_windowCross]
                               : [self displayRect:0 y:_footerStart width:_windowCross height:_footerSize];
    ShadowListKitPlace(_footerView, frame);
  }
}

/*
 * Whether rows the viewport shows are left out of the measured range. The first frame picks the
 * range from estimates, which can stop short of the viewport's end. One more run fills it.
 */
- (BOOL)measuredRangeEndsShort
{
  std::optional<MountedRange> measured = _driver.getMeasuredRange();
  if (!measured || measured->high + 1 >= _driver.getRowCount()) {
    return NO;
  }
  double end = _driver.getLeadingAt(measured->high) + _driver.getExtentAt(measured->high);
  return end < [self offset] + _windowAlong + [self trailingInset];
}

- (void)applyContentSize:(CGFloat)along
{
  _contentAlong = along;
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
    if (_stickyCell) {
      [self unpinCell:_stickyCell];
      _stickyCell = nil;
    }
    return;
  }
  double offset = [self offset];
  std::size_t active = _driver.activeStickyIndex(offset);
  ShadowListKitListCell *cell = active != UNDEFINED_INDEX ? [self mountedCellAtIndex:active] : nil;
  // A held sticky item follows the mouse from its own row.
  if (cell == _heldCell) {
    cell = nil;
  }
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
  if (cell.layer.zPosition != 1) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    cell.layer.zPosition = 1;
    [CATransaction commit];
  }
}

- (void)unpinCell:(ShadowListKitListCell *)cell
{
  if (cell.row != NSNotFound && (std::size_t)cell.row < _driver.getRowCount()) {
    ShadowListKitPlace(cell, [self rowRect:(std::size_t)cell.row]);
  }
  if (cell != _heldCell) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    cell.layer.zPosition = 0;
    [CATransaction commit];
  }
}

#pragma mark - Separators

- (CGColorRef)separatorCGColor
{
  if (!_separatorCGColor) {
    __block CGColorRef color = NULL;
    [self.effectiveAppearance performAsCurrentDrawingAppearance:^{
      color = CGColorRetain(self->_separatorColor.CGColor);
    }];
    _separatorCGColor = color;
  }
  return _separatorCGColor;
}

- (void)dropResolvedColors
{
  CGColorRelease(_separatorCGColor);
  _separatorCGColor = NULL;
}

#pragma mark - Environment

/*
 * Colors resolve again for the new appearance on the next mount pass.
 */
- (void)viewDidChangeEffectiveAppearance
{
  [super viewDidChangeEffectiveAppearance];
  [self dropResolvedColors];
  ++_structureVersion;
  self.needsLayout = YES;
}

/*
 * A new backing scale moves pixel edges and the separator thickness.
 */
- (void)viewDidChangeBackingProperties
{
  [super viewDidChangeBackingProperties];
  ++_structureVersion;
  [self placeContentView];
  [self invalidateFrame];
}

- (void)viewDidEndLiveResize
{
  [super viewDidEndLiveResize];
  [self invalidateFrame];
}

#pragma mark - Scroll events

/*
 * The reader's gesture or a driving host moved the phase. Ending a phase hands the core an idle
 * frame, which lets a held correction go.
 */
- (void)setScrollPhase:(ShadowListKitScrollPhase)scrollPhase
{
  if (scrollPhase == _scrollPhase) {
    return;
  }
  ShadowListKitScrollPhase previous = _scrollPhase;
  _scrollPhase = scrollPhase;
  if (scrollPhase == ShadowListKitScrollPhaseDragging) {
    _driver.cancelLanding();
    [self unhighlight];
    [self closeSwipeAnimated:YES];
    [self updateHover];
    if ([_userDelegate respondsToSelector:@selector(listViewWillBeginDragging:)]) {
      [_userDelegate listViewWillBeginDragging:self];
    }
  }
  if (previous == ShadowListKitScrollPhaseDragging && [_userDelegate respondsToSelector:@selector(listViewDidEndDragging:willDecelerate:)]) {
    [_userDelegate listViewDidEndDragging:self willDecelerate:scrollPhase == ShadowListKitScrollPhaseSettling];
  }
  if (previous == ShadowListKitScrollPhaseSettling && scrollPhase == ShadowListKitScrollPhaseIdle &&
      [_userDelegate respondsToSelector:@selector(listViewDidEndDecelerating:)]) {
    [_userDelegate listViewDidEndDecelerating:self];
  }
  [self invalidateFrame];
  [self invalidateRestorableStateIfSettled];
}

/*
 * What moves the offset now, in the core's words.
 */
- (ScrollPhase)corePhase
{
  return static_cast<ScrollPhase>(_scrollPhase);
}

- (BOOL)isTracking
{
  return _scrollPhase == ShadowListKitScrollPhaseDragging;
}

- (BOOL)isDragging
{
  return _scrollPhase == ShadowListKitScrollPhaseDragging;
}

- (BOOL)isDecelerating
{
  return _scrollPhase == ShadowListKitScrollPhaseSettling;
}

#pragma mark - Queries

/*
 * The shown item cell at a point in the list's coordinates, or nil. A pinned header covers the
 * items under it.
 */
- (ShadowListKitListCell *)itemCellAtPoint:(NSPoint)point
{
  NSPoint content = [_contentView convertPoint:point fromView:self];
  if (_stickyCell && !_stickyCell.hidden && NSPointInRect(content, _stickyCell.frame)) {
    return _stickyCell.index != NSNotFound && _stickyCell.mountGeneration != 0 ? _stickyCell : nil;
  }
  for (auto& entry : _mounted) {
    ShadowListKitListCell *cell = entry.second;
    if (!cell.hidden && cell.index != NSNotFound && cell.mountGeneration != 0 && NSPointInRect(content, cell.frame)) {
      return cell;
    }
  }
  return nil;
}

@end
