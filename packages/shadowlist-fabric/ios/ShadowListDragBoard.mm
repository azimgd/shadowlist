#import "ShadowListDragBoard.h"
#import "ShadowListView+Private.h"

#import <QuartzCore/QuartzCore.h>
#include <TargetConditionals.h>

#include <cmath>
#include <string>
#include <vector>

/*
 * The core's decisions for the board's drag, and the counter that names each list for it.
 * The core needs ids that are unique and not empty. dragListId can be empty or repeated, and
 * each list gets its own token on joining instead.
 */
@interface ShadowListDragBoard () {
@package
  azimgd::shadowlist::DragBoard _core;
  NSUInteger _nextListToken;
}
@end

@implementation ShadowListDragBoard

/*
 * Boards by id. The values are weak because each list holds its board.
 */
static NSMapTable<NSString *, ShadowListDragBoard *> *SLDragBoards(void)
{
  static NSMapTable<NSString *, ShadowListDragBoard *> *boards;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    boards = [NSMapTable strongToWeakObjectsMapTable];
  });
  return boards;
}

+ (ShadowListDragBoard *)boardWithId:(NSString *)boardId
{
  ShadowListDragBoard *board = [SLDragBoards() objectForKey:boardId];
  if (!board) {
    board = [[ShadowListDragBoard alloc] initWithBoardId:boardId];
    [SLDragBoards() setObject:board forKey:boardId];
  }
  return board;
}

- (instancetype)initWithBoardId:(NSString *)boardId
{
  if (self = [super init]) {
    _boardId = [boardId copy];
    _lists = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

/*
 * End the flight right away, without animation: drop the overlay, show the hidden rows and
 * clear the gaps in the home list and the target. A drag still deciding with the core stops
 * there too.
 */
- (void)endFlight
{
  ShadowListView *home = _home;
  ShadowListView *target = _target;
  [_overlay.layer removeAllAnimations];
  [_overlay removeFromSuperview];
  _overlay = nil;
  _home = nil;
  _target = nil;
  _settling = NO;
  _core.endFlight();
  [home dragBoardFlightDidEnd];
  if (target != home) {
    [target dragBoardFlightDidEnd];
  }
}

@end

/*
 * A list's part in a drag board. The home list of the dragged row drives every frame from its
 * drag tick. The target only answers: it places the incoming row, opens the gap and reports
 * where the row would drop.
 */
@implementation ShadowListView (DragBoard)

#if TARGET_OS_OSX

- (void)applyDragBoardId:(NSString *)boardId listId:(NSString *)listId
{
  _dragBoardId = [boardId copy];
  _dragListId = [listId copy];
}

- (void)leaveDragBoard
{
}

- (void)withdrawFromDragBoardFlight
{
}

- (void)dragBoardDidPickUpView:(RCTUIView *)view key:(nullable NSString *)heldKey
{
  (void)view;
  (void)heldKey;
}

- (BOOL)updateDragBoard
{
  return NO;
}

- (BOOL)applyDragBoardAutoScrollWithDt:(double)dtSeconds
{
  (void)dtSeconds;
  return NO;
}

- (BOOL)finishDragBoard
{
  return NO;
}

- (void)cancelDragBoard
{
}

- (void)tearDownDragBoard
{
}

- (void)dragBoardMountDidFinish
{
}

- (void)dragBoardViewWillUnmount:(RCTUIView *)view
{
  (void)view;
}

- (void)dragBoardFlightDidEnd
{
}

#else

#pragma mark - Membership

/*
 * Join the board named by dragBoardId, or leave it when the id is empty or changes.
 */
- (void)applyDragBoardId:(NSString *)boardId listId:(NSString *)listId
{
  _dragListId = [listId copy];
  if (_dragBoardId && [_dragBoardId isEqualToString:boardId]) {
    return;
  }
  [self leaveDragBoard];
  _dragBoardId = [boardId copy];
  if (boardId.length > 0) {
    _dragBoard = [ShadowListDragBoard boardWithId:boardId];
    _dragBoard->_nextListToken += 1;
    _dragBoardToken = std::to_string(_dragBoard->_nextListToken);
    [_dragBoard.lists addObject:self];
  }
}

/*
 * The list on this board with a core id, nil when it left.
 */
- (nullable ShadowListView *)dragBoardListWithToken:(const std::string&)token
{
  if (token.empty()) {
    return nil;
  }
  for (ShadowListView *list in _dragBoard.lists) {
    if (list->_dragBoardToken == token) {
      return list;
    }
  }
  return nil;
}

/*
 * Whether this list is the home of a drag the core is still deciding, before its drop.
 */
- (BOOL)isDragBoardDeciding
{
  ShadowListDragBoard *board = _dragBoard;
  return board && board->_core.isDeciding() && board->_core.getHomeId() == _dragBoardToken;
}

/*
 * Leave the board, on recycle or a new id.
 */
- (void)leaveDragBoard
{
  ShadowListDragBoard *board = _dragBoard;
  [self withdrawFromDragBoardFlight];
  [board.lists removeObject:self];
  if (_dragBoardHiddenView) {
    _dragBoardHiddenView.hidden = NO;
    _dragBoardHiddenView = nil;
  }
  _dragBoardDropPending = NO;
  _dragBoardLandingKey = nil;
  _dragBoardAnimateShifts = NO;
  _dragBoard = nil;
  _dragBoardId = nil;
  _dragBoardToken.clear();
}

/*
 * This list stops taking part in the board's flight, when it leaves the board or the window
 * or turns drag off. As the target of a floating row it closes its gap, and the row floats on
 * without a target until it hovers another list. As the list a dropped row lands in it ends
 * the flight. As the home list of a dropped row it only clears its own part, and the row
 * still lands. Any other flight this list is in ends, and a drag it is home to stops deciding.
 */
- (void)withdrawFromDragBoardFlight
{
  ShadowListDragBoard *board = _dragBoard;
  if (!board) {
    return;
  }
  auto& core = board->_core;
  if (core.isLanding()) {
    if (board.target == self) {
      [board endFlight];
    } else if (board.home == self) {
      board.home = nil;
      [self dragBoardFlightDidEnd];
    }
    return;
  }
  if (board.target == self && board.home != self && core.isFloating() && core.getTargetId() == _dragBoardToken) {
    core.leave(_dragBoardToken);
    board.target = nil;
    [self endDragBoardForeign];
  } else if (board.home == self || board.target == self) {
    [board endFlight];
  } else if ([self isDragBoardDeciding]) {
    core.cancel();
  }
}

#pragma mark - Home list

/*
 * A pickup in a list on a board. A previous flight still settling ends first, and the core
 * starts deciding for this one with the finger's grab on the row. The scroll views around the
 * list stop following the finger, which may cross them on its way to another list: each pan
 * is cancelled in place, and a recognizer ignores a touch that began before it was enabled
 * again. Nothing has to be given back and an app's own scrollEnabled stays as it was.
 */
- (void)dragBoardDidPickUpView:(RCTUIView *)view key:(nullable NSString *)heldKey
{
  ShadowListDragBoard *board = _dragBoard;
  if (!board) {
    return;
  }
  [board endFlight];
  // A row still hidden for a picture flying home shows again.
  if (_dragBoardHiddenView) {
    _dragBoardHiddenView.hidden = NO;
    _dragBoardHiddenView = nil;
  }
  CGRect resting = [self restingFrameForView:view];
  CGPoint center = [_contentView convertPoint:CGPointMake(CGRectGetMidX(resting), CGRectGetMidY(resting)) toView:nil];
  board->_core.begin(
    _dragBoardToken,
    heldKey ? std::string(heldKey.UTF8String) : std::string(),
    {_dragTouchInWindow.x, _dragTouchInWindow.y},
    {center.x, center.y});
  board.windowSize = self.window.bounds.size;
  board.sidewaysScrollView = nil;
  _dragBoardScroll.begin(_dragTouchInWindow.x);
  for (UIPanGestureRecognizer *pan in _ancestorPans) {
    if (pan.enabled) {
      pan.enabled = NO;
      pan.enabled = YES;
    }
  }
}

/*
 * The core ids of the lists of this board under a point in the window, for the core's hover,
 * topmost first. The window's hit test finds the view under the finger. A list covered by
 * another view or another list is not hovered. Lists in another window, hidden, with drag
 * off, busy with a drag of their own, or without a dragListId to name them to JS are left
 * out. The core picks this list where lists nest.
 */
- (std::vector<std::string>)dragBoardCandidatesAtWindowPoint:(CGPoint)point
{
  std::vector<std::string> candidates;
  UIView *hit = [self.window hitTest:point withEvent:nil];
  for (UIView *view = hit; view; view = view.superview) {
    if (![view isKindOfClass:[ShadowListView class]]) {
      continue;
    }
    ShadowListView *list = (ShadowListView *)view;
    if (list->_dragBoard != _dragBoard || list.hidden || !list->_reorderEnabled) {
      continue;
    }
    if (list != self &&
        (!list->_state || list->_dragging || list->_dragDropPending || list->_dragListId.length == 0)) {
      continue;
    }
    candidates.push_back(list->_dragBoardToken);
  }
  return candidates;
}

/*
 * Where the held row's center shows now, in window coordinates.
 */
- (CGPoint)dragBoardShownCenterOfView:(RCTUIView *)view
{
  CGRect resting = [self restingFrameForView:view];
  CGRect shown = CGRectOffset(resting, view.transform.tx, view.transform.ty);
  return [_contentView convertPoint:CGPointMake(CGRectGetMidX(shown), CGRectGetMidY(shown)) toView:nil];
}

/*
 * Every drag frame on a board. The core says what the hover changes and where the picture
 * goes, and this list carries it out. Returns NO while the row is home, where the normal drag
 * goes on. Floating, the overlay follows the finger, this list keeps its gap closed and the
 * target opens one.
 */
- (BOOL)updateDragBoard
{
  ShadowListDragBoard *board = _dragBoard;
  RCTUIView *view = _draggedView;
  if (!board || !view || !self.window || ![self isDragBoardDeciding]) {
    return NO;
  }
  if (!CGSizeEqualToSize(self.window.bounds.size, board.windowSize)) {
    // The window turned or resized under the overlay, which is in window coordinates.
    [self cancelDrag];
    return NO;
  }
  CGPoint finger = [self convertPoint:_dragTouchInViewport toView:nil];
  CGPoint shown = [self dragBoardShownCenterOfView:view];
  azimgd::shadowlist::DragBoardFrame frame = board->_core.frame(
    {finger.x, finger.y}, {shown.x, shown.y}, [self dragBoardCandidatesAtWindowPoint:finger], SLDragNowMs());
  using Kind = azimgd::shadowlist::DragBoardStep::Kind;
  switch (frame.step.kind) {
    case Kind::Stay:
      break;
    case Kind::LiftOut:
      [self liftDragBoardView:view];
      [self targetDragBoardList:[self dragBoardListWithToken:frame.step.next]];
      break;
    case Kind::ReturnHome:
      [self returnDragBoardHome];
      return NO;
    case Kind::Retarget:
      [board.target endDragBoardForeign];
      [self targetDragBoardList:[self dragBoardListWithToken:frame.step.next]];
      break;
  }
  if (!frame.floating || !board.overlay) {
    return NO;
  }
  view.hidden = frame.hideHeld;

  RCTUIView *overlay = board.overlay;
  overlay.center = CGPointMake(frame.overlayCenter.x, frame.overlayCenter.y);
  // The rows after the held one close its gap.
  [self updateDragInsertion];
  [board.target placeDragBoardForeignAtWindowCenter:overlay.center];
  return YES;
}

/*
 * The floating row starts hovering list, which opens a gap for it.
 */
- (void)targetDragBoardList:(nullable ShadowListView *)list
{
  ShadowListDragBoard *board = _dragBoard;
  board.target = list;
  [list beginDragBoardForeignWithSize:board.overlay.bounds.size];
}

/*
 * Lift the held row out of this list. Its picture goes above the window where the row shows
 * now, the row hides, and the rows after it close the gap. The core moves the picture under
 * the finger over the next frames.
 */
- (void)liftDragBoardView:(RCTUIView *)view
{
  ShadowListDragBoard *board = _dragBoard;
  UIWindow *window = self.window;

  CGRect resting = [self restingFrameForView:view];
  CGRect shown = CGRectOffset(resting, view.transform.tx, view.transform.ty);
  CGRect frame = [_contentView convertRect:shown toView:nil];

  /*
   * Draw the row as the screen shows it, which keeps blurs and video that a layer render
   * misses, without its lift shadow. The overlay has its own. Waiting for the screen update
   * takes the shadow off the picture.
   */
  float shadowOpacity = view.layer.shadowOpacity;
  view.layer.shadowOpacity = 0.0;
  UIGraphicsImageRenderer *renderer = [[UIGraphicsImageRenderer alloc] initWithSize:view.bounds.size];
  UIImage *image = [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
    (void)context;
    [view drawViewHierarchyInRect:view.bounds afterScreenUpdates:YES];
  }];
  view.layer.shadowOpacity = shadowOpacity;

  UIView *overlay = [[UIView alloc] initWithFrame:frame];
  overlay.userInteractionEnabled = NO;
  // The same shape as the row's own lift shadow: its card, or its bounds.
  SLApplyDragLiftShadow(view, overlay.layer);
  UIImageView *picture = [[UIImageView alloc] initWithFrame:overlay.bounds];
  picture.image = image;
  picture.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  picture.layer.cornerRadius = view.layer.cornerRadius;
  picture.clipsToBounds = view.layer.cornerRadius > 0.0;
  [overlay addSubview:picture];
  overlay.accessibilityIdentifier = @"shadowlist-drag-board-overlay";
  [window addSubview:overlay];

  board.home = self;
  board.target = nil;
  board.overlay = overlay;

  view.hidden = YES;
  _dragBoardHiddenView = view;
  // In a grid the shifts that close the gap come with the next updateDragInsertion.
  _drag.setHeldAway(true);
}

/*
 * The row came back over this list. It shows again under the finger and the target closes
 * its gap. The rows here slide back on this frame's updateDrag.
 */
- (void)returnDragBoardHome
{
  ShadowListDragBoard *board = _dragBoard;
  ShadowListView *target = board.target;
  board.target = nil;
  [target endDragBoardForeign];
  [board.overlay removeFromSuperview];
  board.overlay = nil;
  board.home = nil;
  _drag.setHeldAway(false);
  _dragBoardAnimateShifts = YES;
  // The next drop spot ticks for the way back.
  _dragFeedback.reset();
  if (_dragBoardHiddenView) {
    _dragBoardHiddenView.hidden = NO;
    _dragBoardHiddenView = nil;
  }
}

/*
 * Scroll for a drag on a board, before the frame's updateDrag. Near the window's left or
 * right edge the scroll view around the lists scrolls sideways. While the row floats, the
 * target scrolls near its own edges instead of this list. Returns YES then.
 */
- (BOOL)applyDragBoardAutoScrollWithDt:(double)dtSeconds
{
  ShadowListDragBoard *board = _dragBoard;
  if (!board || !_dragging || !self.window || ![self isDragBoardDeciding]) {
    return NO;
  }
  CGPoint finger = [self convertPoint:_dragTouchInViewport toView:nil];
  [self scrollDragBoardSidewaysForWindowPoint:finger dtSeconds:dtSeconds];
  if (!board->_core.isFloating() || !board.overlay) {
    return NO;
  }
  ShadowListView *target = board.target;
  if (target) {
    CGPoint touch = [target convertPoint:finger fromView:nil];
    if (CGRectContainsPoint(target.bounds, touch)) {
      [target applyDragAutoScrollForTouch:touch dtSeconds:dtSeconds];
    }
  }
  return YES;
}

/*
 * The range a scroll view's sideways offset can rest in, with its insets.
 */
static void SLSidewaysOffsetRange(UIScrollView *outer, CGFloat *minOffset, CGFloat *maxOffset)
{
  UIEdgeInsets inset = outer.adjustedContentInset;
  *minOffset = -inset.left;
  *maxOffset = MAX(*minOffset, outer.contentSize.width + inset.right - outer.bounds.size.width);
}

/*
 * The nearest scroll view around this list that scrolls sideways moves toward the edge the
 * finger is near. It reveals the next list of a board laid out in a row. The core's
 * DragBoardScroll waits until the finger has moved toward that edge from where it lifted the
 * row, and stops when the finger heads back. A vertical reorder near the edge of a wide
 * column stays put.
 */
- (void)scrollDragBoardSidewaysForWindowPoint:(CGPoint)finger dtSeconds:(double)dtSeconds
{
  for (UIView *ancestor = self.superview; ancestor; ancestor = ancestor.superview) {
    if (![ancestor isKindOfClass:[UIScrollView class]]) {
      continue;
    }
    UIScrollView *outer = (UIScrollView *)ancestor;
    CGFloat minOffset = 0.0;
    CGFloat maxOffset = 0.0;
    SLSidewaysOffsetRange(outer, &minOffset, &maxOffset);
    if (maxOffset <= minOffset) {
      continue;
    }
    CGFloat windowSize = outer.bounds.size.width;
    CGFloat offset = outer.contentOffset.x;
    CGFloat touch = [outer convertPoint:finger fromView:nil].x - offset;
    double delta = _dragBoardScroll.delta(azimgd::shadowlist::DRAG_BOARD_SCROLL_IOS,
      azimgd::shadowlist::DRAG_BOARD_SCROLL_ARM_IOS, finger.x, touch, windowSize, dtSeconds);
    CGFloat next = MIN(MAX(offset + delta, minOffset), maxOffset);
    if (next != offset) {
      outer.contentOffset = CGPointMake(next, outer.contentOffset.y);
      _dragBoard.sidewaysScrollView = outer;
    }
    return;
  }
}

/*
 * Setting the offset never runs a scroll view's paging or React Native's snapToInterval and
 * snapToOffsets. A board the drag scrolled would rest between pages. Move it to the nearest
 * page, interval or offset once the drag ends.
 */
- (void)snapDragBoardScrollView
{
  UIScrollView *outer = _dragBoard.sidewaysScrollView;
  _dragBoard.sidewaysScrollView = nil;
  if (!outer || outer.isDragging || outer.isDecelerating) {
    return;
  }
  CGFloat offset = outer.contentOffset.x;
  CGFloat target = offset;
  CGFloat interval = outer.pagingEnabled ? outer.bounds.size.width : 0.0;
  NSArray<NSNumber *> *offsets = nil;
  if (!interval && [outer respondsToSelector:NSSelectorFromString(@"snapToInterval")]) {
    interval = [[outer valueForKey:@"snapToInterval"] doubleValue];
  }
  if (!interval && [outer respondsToSelector:NSSelectorFromString(@"snapToOffsets")]) {
    offsets = [outer valueForKey:@"snapToOffsets"];
  }
  if (interval > 0.0) {
    target = std::round(offset / interval) * interval;
  } else if (offsets.count > 0) {
    CGFloat best = offsets.firstObject.doubleValue;
    for (NSNumber *candidate in offsets) {
      if (std::fabs(candidate.doubleValue - offset) < std::fabs(best - offset)) {
        best = candidate.doubleValue;
      }
    }
    target = best;
  } else {
    return;
  }
  CGFloat minOffset = 0.0;
  CGFloat maxOffset = 0.0;
  SLSidewaysOffsetRange(outer, &minOffset, &maxOffset);
  target = MIN(MAX(target, minOffset), maxOffset);
  if (std::fabs(target - offset) >= 0.5) {
    [outer setContentOffset:CGPointMake(target, outer.contentOffset.y) animated:YES];
  }
}

/*
 * Let go on a board. The core decides with the target's drop spot. A row let go while home
 * returns NO, and the normal drop runs. A floating row drops into the target, which is the
 * last list hovered, even when the finger is now between lists. The target hears the landing
 * first, which tells its core to keep the offset when the row arrives. Then this list sends
 * onDragEnd with the target's list id, the index and the key of the row the moved one goes
 * before, and both lists keep their gaps until the data change lands. The row goes back home
 * only when there is no target, the target has no drop spot or the row has no key.
 */
- (BOOL)finishDragBoard
{
  ShadowListDragBoard *board = _dragBoard;
  if (!board || ![self isDragBoardDeciding]) {
    return NO;
  }
  [self snapDragBoardScrollView];
  auto& core = board->_core;
  ShadowListView *target = board.target;
  BOOL foreign = target && target->_drag.isForeign();
  azimgd::shadowlist::DragBoardRelease release = core.release(
    foreign ? target->_drag.getInsertionIndex() : azimgd::shadowlist::UNDEFINED_INDEX,
    foreign ? target->_drag.getInsertionKey() : std::string(),
    foreign ? [target rowKeyCount] : azimgd::shadowlist::UNDEFINED_INDEX,
    SLDragNowMs());
  using Kind = azimgd::shadowlist::DragBoardRelease::Kind;
  if (release.kind == Kind::Home) {
    return NO;
  }
  ShadowListView *destination = [self dragBoardListWithToken:release.listId];
  if (release.kind == Kind::Cancel || !destination || destination != target) {
    core.endFlight();
    [self cancelDrag];
    return YES;
  }

  [_dragDisplayLink invalidate];
  _dragDisplayLink = nil;
  _scrollView.scrollEnabled = _scrollEnabled;

  NSString *heldKey = SLDragKey(core.getHeldKey());
  _dragging = NO;
  _draggedView = nil;
  _dragBoardDropPending = YES;
  target->_dragBoardLandingKey = heldKey;

  [target commitDragEventType:azimgd::shadowlist::DRAG_EVENT_LAND sourceKey:heldKey destinationKey:heldKey];
  [self commitDragEventType:azimgd::shadowlist::DRAG_EVENT_END
                  sourceKey:heldKey
             destinationKey:SLDragKey(release.key)
          destinationListId:target->_dragListId ?: @""
           destinationIndex:(NSInteger)release.index];

  /*
   * The move may never land, like when JS has no onMoveItemToList. Clean up anyway: a drop
   * still waiting has expired, and a row that landed off screen never mounts to settle on.
   */
  std::uint64_t flight = core.getFlight();
  __weak ShadowListDragBoard *weakBoard = board;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                   (int64_t)((azimgd::shadowlist::DRAG_BOARD_LAND_TIMEOUT_MS + 50.0) * NSEC_PER_MSEC)),
    dispatch_get_main_queue(), ^{
      ShadowListDragBoard *strongBoard = weakBoard;
      if (!strongBoard || strongBoard->_core.getFlight() != flight) {
        return;
      }
      BOOL settledOffScreen =
        strongBoard->_core.getPhase() == azimgd::shadowlist::DragBoardPhase::Settling && !strongBoard.settling;
      if (strongBoard->_core.expired(SLDragNowMs()) || settledOffScreen) {
        [strongBoard endFlight];
      }
    });
  return YES;
}

/*
 * A cancel. The core stops deciding. While the row floats the target closes its gap, and the
 * picture flies back onto the row in this list, which shows again when it arrives. cancelDrag
 * calls this before tearing down the drag, which slides this list's rows back.
 */
- (void)cancelDragBoard
{
  ShadowListDragBoard *board = _dragBoard;
  if (!board) {
    return;
  }
  if ([self isDragBoardDeciding]) {
    [self snapDragBoardScrollView];
    board->_core.cancel();
  }
  if (board.home != self || !board.overlay || board->_core.isLanding()) {
    return;
  }
  RCTUIView *overlay = board.overlay;
  ShadowListView *target = board.target;
  // The flight ends now. Only the picture is left, flying home.
  board.overlay = nil;
  board.home = nil;
  board.target = nil;
  [target endDragBoardForeign];
  _drag.setHeldAway(false);

  RCTUIView *home = _dragBoardHiddenView;
  if (!home || !home.window) {
    [overlay removeFromSuperview];
    home.hidden = NO;
    _dragBoardHiddenView = nil;
    return;
  }
  CGRect frame = [_contentView convertRect:[self restingFrameForView:home] toView:nil];
  __weak ShadowListView *weakSelf = self;
  [UIView animateWithDuration:SL_DROP_SETTLE_DURATION
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseOut | UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     overlay.frame = frame;
                   }
                   completion:^(BOOL finished) {
                     (void)finished;
                     [overlay removeFromSuperview];
                     ShadowListView *strongSelf = weakSelf;
                     if (strongSelf && strongSelf->_dragBoardHiddenView == home) {
                       home.hidden = NO;
                       strongSelf->_dragBoardHiddenView = nil;
                     }
                   }];
}

/*
 * A drag stops without a drop, on recycle or cancel. See withdrawFromDragBoardFlight.
 */
- (void)tearDownDragBoard
{
  [self withdrawFromDragBoardFlight];
}

#pragma mark - Target list

/*
 * A row from another list starts hovering this one, with its size in the window.
 */
- (void)beginDragBoardForeignWithSize:(CGSize)size
{
  // The first drop spot here ticks for the move into this list.
  _dragFeedback.reset();
  _drag.beginForeign(
    _horizontal ? size.width : size.height,
    _horizontal ? size.height : size.width,
    (std::size_t)MAX(_numberOfColumns, 1));
}

/*
 * Place the incoming row's center, in window coordinates, and open the gap where it would
 * drop.
 */
- (void)placeDragBoardForeignAtWindowCenter:(CGPoint)center
{
  if (!_drag.isForeign()) {
    return;
  }
  CGPoint content = [_contentView convertPoint:center fromView:nil];
  _drag.placeForeignCenter(_horizontal ? content.x : content.y, _horizontal ? content.y : content.x);
  [self updateDragInsertion];
}

/*
 * The incoming row left this list. The rows slide back.
 */
- (void)endDragBoardForeign
{
  if (!_drag.isForeign()) {
    return;
  }
  _drag.endForeign();
  _dragBoardAnimateShifts = YES;
  [self applyDragShuffle];
  _dragBoardAnimateShifts = NO;
}

#pragma mark - Landing

- (nullable RCTUIView *)dragBoardCellViewForKey:(NSString *)key
{
  for (RCTUIView *subview in _contentView.subviews) {
    if ([[self keyOfCellView:subview] isEqualToString:key]) {
      return subview;
    }
  }
  return nil;
}

/*
 * After every mount. The home list clears its gap once the row has left its data. The target
 * clears its gap as soon as its data holds the row, since its rows then rest in their new
 * places and a gap kept by index would shift them twice. The picture settles onto the row
 * once its cell mounts, in the same mount or a later one.
 */
- (void)dragBoardMountDidFinish
{
  ShadowListDragBoard *board = _dragBoard;
  if (!board) {
    return;
  }
  auto& core = board->_core;
  NSString *heldKey = core.getHeldKey().empty() ? nil : SLDragKey(core.getHeldKey());
  BOOL hiddenHoldsHeldRow = _dragBoardHiddenView && heldKey &&
    [[self keyOfCellView:_dragBoardHiddenView] isEqualToString:heldKey];
  if (_dragBoardHiddenView && !hiddenHoldsHeldRow) {
    _dragBoardHiddenView.hidden = NO;
    _dragBoardHiddenView = nil;
    // The floating row's cell now shows another row. The row it held is gone. Stop the drag.
    if (board.home == self && core.isFloating() && _dragging) {
      [self cancelDrag];
      return;
    }
  }
  if (_dragBoardDropPending && (!heldKey || (![self rowKeysContainKey:heldKey] && ![self dragBoardCellViewForKey:heldKey]))) {
    _dragBoardDropPending = NO;
    if (!_dragging) {
      [self clearDragTransforms];
    }
  }
  if (!_dragBoardLandingKey) {
    return;
  }
  if (core.getPhase() == azimgd::shadowlist::DragBoardPhase::Landing &&
      core.landed(_dragBoardToken, [self rowKeysContainKey:_dragBoardLandingKey])) {
    _drag.endForeign();
    [self clearDragTransforms];
  }
  if (core.getPhase() == azimgd::shadowlist::DragBoardPhase::Settling && core.getTargetId() == _dragBoardToken &&
      !board.settling) {
    RCTUIView *cell = [self dragBoardCellViewForKey:_dragBoardLandingKey];
    if (cell) {
      [self landDragBoardOnView:cell];
    }
  }
}

- (void)landDragBoardOnView:(RCTUIView *)cell
{
  ShadowListDragBoard *board = _dragBoard;
  _dragBoardLandingKey = nil;
  RCTUIView *overlay = board.overlay;
  if (!overlay || !cell.window) {
    [board endFlight];
    return;
  }
  board.settling = YES;
  cell.hidden = YES;
  _dragBoardHiddenView = cell;
  CGRect frame = [_contentView convertRect:[self restingFrameForView:cell] toView:nil];
  std::uint64_t flight = board->_core.getFlight();
  __weak ShadowListDragBoard *weakBoard = board;
  [UIView animateWithDuration:SL_DROP_SETTLE_DURATION
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseOut | UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     overlay.frame = frame;
                   }
                   completion:^(BOOL finished) {
                     (void)finished;
                     ShadowListDragBoard *strongBoard = weakBoard;
                     if (strongBoard && strongBoard->_core.getFlight() == flight) {
                       [strongBoard endFlight];
                     }
                   }];
}

/*
 * A row the board hid shows again before React reuses it.
 */
- (void)dragBoardViewWillUnmount:(RCTUIView *)view
{
  if (view == _dragBoardHiddenView) {
    view.hidden = NO;
    _dragBoardHiddenView = nil;
  }
}

/*
 * The flight ended. Show the hidden row and clear what this list held for it.
 */
- (void)dragBoardFlightDidEnd
{
  if (_dragBoardHiddenView) {
    _dragBoardHiddenView.hidden = NO;
    _dragBoardHiddenView = nil;
  }
  _dragBoardLandingKey = nil;
  if (_drag.isForeign()) {
    _drag.endForeign();
    [self clearDragTransforms];
  }
  if (_dragBoardDropPending) {
    _dragBoardDropPending = NO;
    if (!_dragging) {
      [self clearDragTransforms];
    }
  }
  if (_dragging && _drag.isHeldAway()) {
    _drag.setHeldAway(false);
  }
}

#endif

@end
