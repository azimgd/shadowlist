#import "Internal/ShadowListKitListView+Private.h"
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
  &ShadowListKitListViewActionsLink,
  &ShadowListKitListViewCommandsLink,
  &ShadowListKitListViewDataLink,
  &ShadowListKitListViewDragLink,
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
  ShadowListKitContentView *_contentView;

  NSUInteger _mountGeneration;

  /*
   * What the last mount pass covered. The pass is skipped while all of it holds.
   */
  std::size_t _mountedLow;
  std::size_t _mountedHigh;
  std::size_t _mountedSticky;
  std::uint64_t _mountedGeometry;
  NSUInteger _mountedStructure;

  NSMutableDictionary<NSString *, Class> *_cellClasses;
  NSMutableDictionary<NSString *, NSMutableArray<ShadowListKitListCell *> *> *_reusePool;

  /*
   * The band of offsets where the core has nothing to do.
   */
  OffsetBand _band;
  BOOL _needsFrame;

  CGFloat _headerSize;
  CGFloat _footerSize;
  CGFloat _contentAlong;
  double _footerStart;

  /*
   * The settings the core runs with, sent again only when one changed.
   */
  ListSettings _sentSettings;
  BOOL _hasSentSettings;

  ShadowListKitPassGeometry _passGeometry;

  /*
   * The content offset along the axis, exact. The content view shows it on device pixels and
   * rows are drawn shifted by the difference, which keeps the content where the core holds it.
   */
  double _offsetAlong;

  /*
   * The last offset seen, to tell the user's scrolling from our own writes.
   */
  double _previousOffset;
  /*
   * The pixel shift the last mount pass placed rows with.
   */
  double _mountedShift;
  BOOL _userScrolled;

  BOOL _inLayoutPass;
  /*
   * Set while the pass after a clamp to the end runs.
   */
  BOOL _clampingPass;
  /*
   * A settle frame waits for the next display frame, at most one at a time.
   */
  BOOL _settleScheduled;
  CADisplayLink *_settleLink;
  BOOL _reachedStart;
  BOOL _reachedEnd;

  /*
   * The separator color as the list's appearance shows it, resolved on first use.
   */
  CGColorRef _separatorCGColor;

  std::vector<std::size_t> _prefetchRows;
  std::vector<std::size_t> _cancelRows;
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
  if (_dragKeyMonitor) {
    [NSEvent removeMonitor:_dragKeyMonitor];
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
  _delegate = delegate;
}

- (id<ShadowListKitListViewDelegate>)delegate
{
  return _delegate;
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
  if ([_delegate respondsToSelector:@selector(listView:didCorrectOffset:)]) {
    [_delegate listView:self didCorrectOffset:point];
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
  if ([_delegate respondsToSelector:@selector(listViewDidScroll:)]) {
    [_delegate listViewDidScroll:self];
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
  if (animated && [_delegate respondsToSelector:@selector(listViewDidEndScrollingAnimation:)]) {
    [_delegate listViewDidEndScrollingAnimation:self];
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
    [self updateHover];
    if ([_delegate respondsToSelector:@selector(listViewWillBeginDragging:)]) {
      [_delegate listViewWillBeginDragging:self];
    }
  }
  if (previous == ShadowListKitScrollPhaseDragging && [_delegate respondsToSelector:@selector(listViewDidEndDragging:willDecelerate:)]) {
    [_delegate listViewDidEndDragging:self willDecelerate:scrollPhase == ShadowListKitScrollPhaseSettling];
  }
  if (previous == ShadowListKitScrollPhaseSettling && scrollPhase == ShadowListKitScrollPhaseIdle &&
      [_delegate respondsToSelector:@selector(listViewDidEndDecelerating:)]) {
    [_delegate listViewDidEndDecelerating:self];
  }
  [self invalidateFrame];
  [self invalidateRestorableStateIfSettled];
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

/*
 * Run the core on the next layout pass even inside the band.
 */
- (void)invalidateFrame
{
  _needsFrame = YES;
  self.needsLayout = YES;
}

- (ListSettings)listSettings
{
  ListSettings settings;
  settings.estimatedRowSize = _estimatedItemSize;
  settings.overscan = _overscan;
  settings.startReachedThreshold = _startReachedThreshold;
  settings.endReachedThreshold = _endReachedThreshold;
  settings.numberOfColumns = (std::size_t)_numberOfColumns;
  settings.inverted = _inverted;
  settings.followAppends = _followAppends;
  settings.horizontal = _horizontal;
  settings.snapToItem = _snapToItem;
  settings.snapAlignment = static_cast<SnapAlignment>(_snapAlignment);
  return settings;
}

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
  [_contentView addSubview:cell];
  return cell;
}

- (void)recycleCell:(ShadowListKitListCell *)cell
{
  // A cell only measured was never displayed.
  NSInteger index = cell.mountGeneration != 0 ? cell.index : NSNotFound;
  cell.mountGeneration = 0;
  [cell.layer removeAllAnimations];
  cell.alphaValue = 1;
  cell.hidden = YES;
  cell.index = NSNotFound;
  cell.row = NSNotFound;
  cell.separatorLayer.hidden = YES;
  if (cell == _stickyCell) {
    _stickyCell = nil;
  }
  if (cell.isHighlighted) {
    [cell setHighlighted:NO animated:NO];
  }
  if (cell.isSelected) {
    [cell setSelected:NO animated:NO];
  }
  if (cell.isHovered) {
    [cell setHovered:NO animated:NO];
  }
  if (cell.isMenuTarget) {
    [self endMenuTarget];
  }
  [self resetDragLookOfCell:cell];
  if (index != NSNotFound && [_delegate respondsToSelector:@selector(listView:didEndDisplayingCell:atIndex:)]) {
    [_delegate listView:self didEndDisplayingCell:cell atIndex:index];
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
  if (cell.superview != _contentView) {
    [_contentView addSubview:cell];
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
  cell.label.stringValue = (footer ? [_dataSource listView:self titleForFooterInSection:section]
                                   : [_dataSource listView:self titleForHeaderInSection:section]) ?: @"";
  cell.needsLayout = YES;
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
  }
  _passGeometry.valid = false;
  _inLayoutPass = NO;
  [self scrollGeometryChanged];
  // The content may have moved under a still pointer.
  if (_hoverActive || _hoveredCell) {
    [self updateHover];
  }
  [self dispatchReached];
  if ([_delegate respondsToSelector:@selector(listViewDidLayOut:)]) {
    [_delegate listViewDidLayOut:self];
  }
}

/*
 * Read the viewport and template sizes. Returns whether any changed since the last pass. A new
 * cross size drops every size and keeps the first visible row, live resizing included.
 */
- (BOOL)readGeometry
{
  NSSize bounds = self.bounds.size;
  NSEdgeInsets inset = self.adjustedContentInset;
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
  if (_scrollPhase != ShadowListKitScrollPhaseIdle) {
    _userScrolled = YES;
  }
  _previousOffset = current;
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
  input.phase = static_cast<ScrollPhase>(_scrollPhase);
  input.userScrolled = _userScrolled;
  input.tracking = self.isTracking;
  // The mount pass's padding, which the core prefetches around before it measures.
  input.prefetch = _prefetchDataSource != nil;
  input.mountPad = _windowAlong * _mountOverscan;
  input.leadingInset = [self leadingInset];
  input.trailingInset = [self trailingInset];
  PassResult result = _driver.runPasses(input);
  _userScrolled = NO;
  _needsFrame = NO;
  [self applyPassResult:result];
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

/*
 * The content size goes first. The offset write is then inside the scroll range.
 */
- (void)applyPassResult:(const PassResult&)result
{
  [self applyContentSize:(CGFloat)result.contentAlong];
  /*
   * Content that shrank under a resting view clamps it to the new end. The core leaves that
   * to the host for inverted lists. It runs again on the clamped offset before the frame is
   * drawn. A drag past the end is left to the gesture.
   */
  if (result.offset > [self maxOffset] + 0.5 && _scrollPhase == ShadowListKitScrollPhaseIdle && !_clampingPass) {
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

- (void)applyContentSize:(CGFloat)along
{
  _contentAlong = along;
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
    ShadowListKitSettleTarget *target = [ShadowListKitSettleTarget new];
    target.list = self;
    _settleLink = [self displayLinkWithTarget:target selector:@selector(tick:)];
    [_settleLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
  }
  _settleLink.paused = NO;
}

- (void)settleFrame
{
  _settleLink.paused = YES;
  _settleScheduled = NO;
  self.needsLayout = YES;
  [self layoutSubtreeIfNeeded];
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
 * which keeps the content of a cell that only moved. Rows under the insets show and are
 * mounted too.
 */
- (void)mountCells
{
  if (!_driver.getMeasuredRange()) {
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
  // The held row stays mounted wherever the mouse takes it.
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
  ShadowListKitPlace(cell, [self rowRect:index]);
  [self placeSeparatorOfCell:cell row:index];
  if (!appearing) {
    return;
  }
  cell.hidden = NO;
  [self applyStateToCell:cell key:key];
  if (cell.index != NSNotFound && [_delegate respondsToSelector:@selector(listView:willDisplayCell:atIndex:)]) {
    [_delegate listView:self willDisplayCell:cell atIndex:cell.index];
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
      [self recycleCell:entry->second];
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
  if (!_prefetchDataSource) {
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
  id<ShadowListKitListViewPrefetchDataSource> prefetch = _prefetchDataSource;
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
  NSSize fits = [cell sizeThatFits:_horizontal ? NSMakeSize(CGFLOAT_MAX, cross) : NSMakeSize(cross, CGFLOAT_MAX)];
  return _horizontal ? fits.width : fits.height;
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

/*
 * The line over an item's trailing edge when another item of its section follows. Grids have
 * none.
 */
- (void)placeSeparatorOfCell:(ShadowListKitListCell *)cell row:(std::size_t)row
{
  BOOL shows = _showsSeparators && _numberOfColumns == 1 && row + 1 < _driver.getKeyCount() &&
    _sections.isItemBeforeItem(row);
  if (shows && cell.index != NSNotFound && [_delegate respondsToSelector:@selector(listView:showsSeparatorAfterItemAtIndex:)]) {
    shows = [_delegate listView:self showsSeparatorAfterItemAtIndex:cell.index];
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
  line.backgroundColor = [self separatorCGColor];
  [CATransaction commit];
}

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

#pragma mark - Events

- (void)dispatchReached
{
  if (_reachedStart) {
    _reachedStart = NO;
    if ([_delegate respondsToSelector:@selector(listViewDidReachStart:)]) {
      [_delegate listViewDidReachStart:self];
    }
  }
  if (_reachedEnd) {
    _reachedEnd = NO;
    if ([_delegate respondsToSelector:@selector(listViewDidReachEnd:)]) {
      [_delegate listViewDidReachEnd:self];
    }
  }
}

#pragma mark - Queries

- (ShadowListKitListCell *)cellForItemAtIndex:(NSInteger)index
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound) {
    return nil;
  }
  ShadowListKitListCell *cell = [self mountedCellAtIndex:(std::size_t)row];
  return cell.hidden ? nil : cell;
}

- (NSArray<ShadowListKitListCell *> *)visibleCells
{
  NSMutableArray<ShadowListKitListCell *> *cells = [NSMutableArray arrayWithCapacity:_mounted.size()];
  NSRect visible = _contentView.bounds;
  for (auto& entry : _mounted) {
    ShadowListKitListCell *cell = entry.second;
    if (!cell.hidden && cell.index != NSNotFound && NSIntersectsRect(cell.frame, visible)) {
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

- (CGRect)rectForItemAtIndex:(NSInteger)index
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound || (std::size_t)row >= _driver.getRowCount()) {
    return CGRectNull;
  }
  return [self rowRect:(std::size_t)row];
}

@end
