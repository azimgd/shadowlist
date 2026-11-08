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
 * How long after the last move of a driven list its views take their layers' geometry.
 */
static const NSTimeInterval SHADOWLIST_KIT_VIEW_CATCH_UP_DELAY = 0.1;

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

#pragma mark - List

// The categories in ShadowListKitListView+*.mm implement the rest of the class interface.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wincomplete-implementation"
@implementation ShadowListKitListView {
  /*
   * The content offset along the axis, exact. The content view shows it on device pixels and
   * rows are drawn shifted by the difference, which keeps the content where the core holds it.
   */
  double _offsetAlong;

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
  [_catchUpTimer invalidate];
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
 * pixel. Rows are drawn this much earlier. A driven list draws rows on their own device pixels,
 * which leaves them in place while only the offset moves.
 */
- (double)pixelShift
{
  if (_passGeometry.valid) {
    return _passGeometry.shift;
  }
  return _scrollDriven ? 0.0 : _offsetAlong - [self pixelAligned:_offsetAlong];
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
 * the pass read its geometry again. A driven list lays out as its host moves it, unless it waits
 * for a layout already.
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
  if (!_inLayoutPass && ![self movedWithoutPass]) {
    if (_scrollDriven && !self.needsLayout) {
      [self layoutPass];
    } else {
      self.needsLayout = YES;
    }
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
 * While a driven list places its cells by their layers, the bounds origin keeps its place along
 * the axis and the layer's sublayer transform moves the content by the rest.
 */
- (void)placeContentView
{
  NSRect frame = self.bounds;
  CGPoint offset = self.contentOffset;
  CGFloat along = [self pixelAligned:[self along:offset]];
  NSPoint origin = _horizontal ? NSMakePoint(along, offset.y) : NSMakePoint(offset.x, along);
  CGFloat shift = 0;
  if ([self placesByLayer]) {
    NSPoint kept = _contentView.bounds.origin;
    shift = [self along:kept] - along;
    origin = _horizontal ? NSMakePoint(kept.x, origin.y) : NSMakePoint(origin.x, kept.y);
  }
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  if (!NSEqualRects(_contentView.frame, frame)) {
    _contentView.frame = frame;
  }
  if (!NSEqualPoints(_contentView.bounds.origin, origin)) {
    [_contentView setBoundsOrigin:origin];
  }
  if (shift != _contentShift) {
    _contentShift = shift;
    _contentView.layer.sublayerTransform =
      _horizontal ? CATransform3DMakeTranslation(shift, 0, 0) : CATransform3DMakeTranslation(0, shift, 0);
    if (shift != 0) {
      [self noteViewsBehind];
    }
  }
  [CATransaction commit];
}

/*
 * Whether a driven list moves its content and cells by their layers now. A held row, an open
 * swipe and change animations read and move the views, which then place them.
 */
- (BOOL)placesByLayer
{
  return _scrollDriven && !_changes && !_swipeCell && ![self hasHeldRow];
}

/*
 * A view lags behind its layer. Once the list rests the views take the layers' geometry.
 */
- (void)noteViewsBehind
{
  _viewsBehind = YES;
  NSDate *rest = [NSDate dateWithTimeIntervalSinceNow:SHADOWLIST_KIT_VIEW_CATCH_UP_DELAY];
  if (_catchUpTimer.valid) {
    _catchUpTimer.fireDate = rest;
    return;
  }
  __weak ShadowListKitListView *weakSelf = self;
  _catchUpTimer = [[NSTimer alloc] initWithFireDate:rest interval:0 repeats:NO block:^(NSTimer *timer) {
    [weakSelf catchUpViews];
  }];
  [NSRunLoop.mainRunLoop addTimer:_catchUpTimer forMode:NSDefaultRunLoopMode];
}

/*
 * Every cell and the content view take their layers' geometry. Nothing moves on screen.
 */
- (void)catchUpViews
{
  if (!_viewsBehind) {
    return;
  }
  _viewsBehind = NO;
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  for (NSView *view in _contentView.subviews) {
    if ([view isKindOfClass:[ShadowListKitListCell class]]) {
      [(ShadowListKitListCell *)view catchUpView];
    }
  }
  if (_contentShift != 0) {
    NSPoint origin = _contentView.bounds.origin;
    if (_horizontal) {
      origin.x -= _contentShift;
    } else {
      origin.y -= _contentShift;
    }
    _contentShift = 0;
    _contentView.layer.sublayerTransform = CATransform3DIdentity;
    [_contentView setBoundsOrigin:origin];
  }
  [CATransaction commit];
}

- (CGPoint)contentPointAt:(CGPoint)point
{
  NSPoint content = [_contentView convertPoint:point fromView:self];
  if (_horizontal) {
    content.x -= _contentShift;
  } else {
    content.y -= _contentShift;
  }
  return content;
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
  [self catchUpViews];
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
  [self catchUpViews];
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

/*
 * A driven list moves a cell that keeps its size by its layer. A cell of a new size, or one
 * placed while views place the cells, catches up and takes its frame.
 */
- (void)placeCell:(ShadowListKitListCell *)cell frame:(CGRect)frame
{
  if ([self placesByLayerCell:cell] && NSEqualSizes(cell.frame.size, frame.size)) {
    if (!NSEqualPoints(cell.shownFrame.origin, frame.origin)) {
      [cell placeLayerAtOrigin:frame.origin];
      [self noteViewsBehind];
    }
    return;
  }
  [cell catchUpView];
  ShadowListKitPlace(cell, frame);
}

/*
 * A driven list hides and shows a cell by its layer. A cell whose view is hidden (a new one, or
 * one recycled while views placed the cells) is shown by its view: AppKit draws no hidden view,
 * and its layer would show the row it held before, or nothing.
 */
- (void)setCell:(ShadowListKitListCell *)cell hidden:(BOOL)hidden
{
  if ([self placesByLayerCell:cell] && (hidden || !cell.isHidden)) {
    if (cell.shown == hidden) {
      [cell setLayerHidden:hidden];
      [self noteViewsBehind];
    }
    return;
  }
  [cell catchUpView];
  cell.hidden = hidden;
}

/*
 * A cell made without a reuse identifier leaves the list when recycled, with its view in place.
 */
- (BOOL)placesByLayerCell:(ShadowListKitListCell *)cell
{
  return [self placesByLayer] && cell.reuseIdentifier != nil && [cell canPlaceByLayer];
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
 * A driven list moved inside the core's band and the band of its mount plan, with nothing else
 * waiting. The content moved already, the same rows stay mounted and no pass runs. Returns
 * whether that was the case.
 */
- (BOOL)movedWithoutPass
{
  if (!_scrollDriven || _needsFrame || self.needsLayout || _windowAlong <= 0 || _windowCross <= 0) {
    return NO;
  }
  if (_driver.hasSticky() || [self hasHeldRow] || _swipeCell || _changes || _hoverActive || _hoveredCell) {
    return NO;
  }
  double offset = [self offset];
  if (!_band.contains(offset) || !_mountBand.contains(offset) || _driver.getGeometryVersion() != _mountedGeometry ||
      _structureVersion != _mountedStructure) {
    return NO;
  }
  [self trackUserScroll];
  return YES;
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

- (void)applyContentSize:(CGFloat)along
{
  _contentAlong = along;
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
  [self catchUpViews];
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
 * items under it. Cells are where their layers show them.
 */
- (ShadowListKitListCell *)itemCellAtPoint:(NSPoint)point
{
  NSPoint content = [self contentPointAt:point];
  if (_stickyCell && _stickyCell.shown && NSPointInRect(content, _stickyCell.shownFrame)) {
    return _stickyCell.index != NSNotFound && _stickyCell.mountGeneration != 0 ? _stickyCell : nil;
  }
  for (auto& entry : _mounted) {
    ShadowListKitListCell *cell = entry.second;
    if (cell.shown && cell.index != NSNotFound && cell.mountGeneration != 0 &&
        NSPointInRect(content, cell.shownFrame)) {
      return cell;
    }
  }
  return nil;
}

/*
 * A view hidden while its layer shows takes no hit test of its own. Its subviews do, and the
 * view takes the rest.
 */
- (NSView *)hitTestHiddenView:(NSView *)view at:(NSPoint)point
{
  NSPoint local = [view convertPoint:point fromView:view.superview];
  for (NSView *subview in view.subviews.reverseObjectEnumerator) {
    NSView *hit = [subview hitTest:local];
    if (hit) {
      return hit;
    }
  }
  return view;
}

/*
 * A hit test while views lag behind their layers: the view under a point where the layers show
 * it. The pinned row covers the rows under it.
 */
- (NSView *)hitTestThroughLayers:(NSPoint)point
{
  NSPoint local = [self convertPoint:point fromView:self.superview];
  if (!NSMouseInRect(local, self.bounds, self.flipped)) {
    return nil;
  }
  for (NSView *view in self.subviews.reverseObjectEnumerator) {
    if (view != _contentView) {
      NSView *hit = view.hidden ? nil : [view hitTest:local];
      if (hit) {
        return hit;
      }
      continue;
    }
    NSPoint content = [self contentPointAt:local];
    NSMutableArray<NSView *> *views = [NSMutableArray arrayWithCapacity:_contentView.subviews.count + 1];
    if (_stickyCell) {
      [views addObject:_stickyCell];
    }
    [views addObjectsFromArray:_contentView.subviews.reverseObjectEnumerator.allObjects];
    for (NSView *subview in views) {
      BOOL isCell = [subview isKindOfClass:[ShadowListKitListCell class]];
      ShadowListKitListCell *cell = isCell ? (ShadowListKitListCell *)subview : nil;
      NSRect shown = cell ? cell.shownFrame : subview.frame;
      if ((cell ? !cell.shown : subview.hidden) || !NSMouseInRect(content, shown, YES)) {
        continue;
      }
      NSRect frame = subview.frame;
      NSPoint place = NSMakePoint(content.x - NSMinX(shown) + NSMinX(frame), content.y - NSMinY(shown) + NSMinY(frame));
      NSView *hit = subview.hidden ? [self hitTestHiddenView:subview at:place] : [subview hitTest:place];
      if (hit) {
        return hit;
      }
    }
    return _contentView;
  }
  return self;
}

@end
