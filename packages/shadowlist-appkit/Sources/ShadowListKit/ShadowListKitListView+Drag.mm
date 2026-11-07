#import "Internal/ShadowListKitListView+Private.h"

#import <ShadowListKit/ShadowListKitListView+Testing.h>

#include <cmath>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewDragLink = 0;

/*
 * Scale of a held row and the durations of the drag, from the core's DragReorder.
 */
static const CGFloat SHADOWLIST_KIT_LIFT_SCALE = DRAG_LIFT_SCALE;
static const NSTimeInterval SHADOWLIST_KIT_SHIFT_DURATION = DRAG_SHIFT_DURATION_MS / 1000.0;
static const NSTimeInterval SHADOWLIST_KIT_DROP_DURATION = DRAG_DROP_DURATION_MS / 1000.0;

/*
 * How far the mouse drags a pressed item before it lifts, in points. A shorter press is a click.
 */
static const CGFloat SHADOWLIST_KIT_DRAG_SLOP = 3;

static const unsigned short SHADOWLIST_KIT_ESCAPE_KEY_CODE = 53;

static NSString *const SHADOWLIST_KIT_TRANSFORM_KEY = @"transform";

/*
 * Set a layer's transform, sliding to it over duration from where it shows now. Without a
 * duration it lands at once and a slide still running stops.
 */
static void ShadowListKitSetTransform(CALayer *layer, CATransform3D transform, NSTimeInterval duration)
{
  CALayer *shown = [layer animationForKey:SHADOWLIST_KIT_TRANSFORM_KEY] ? layer.presentationLayer : nil;
  CATransform3D from = shown ? shown.transform : layer.transform;
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [layer removeAnimationForKey:SHADOWLIST_KIT_TRANSFORM_KEY];
  layer.transform = transform;
  if (duration > 0 && !CATransform3DEqualToTransform(from, transform)) {
    CABasicAnimation *slide = [CABasicAnimation animationWithKeyPath:SHADOWLIST_KIT_TRANSFORM_KEY];
    slide.fromValue = [NSValue valueWithCATransform3D:from];
    slide.toValue = [NSValue valueWithCATransform3D:transform];
    slide.duration = duration;
    slide.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
    [layer addAnimation:slide forKey:SHADOWLIST_KIT_TRANSFORM_KEY];
  }
  [CATransaction commit];
}

static void ShadowListKitPerformHaptic(NSHapticFeedbackPattern pattern)
{
  [NSHapticFeedbackManager.defaultPerformer performFeedbackPattern:pattern
                                                    performanceTime:NSHapticFeedbackPerformanceTimeNow];
}

@interface ShadowListKitListView (DragSteps)
- (void)setDragTouch:(CGPoint)location;
- (void)beginDragAtPoint:(CGPoint)location;
- (void)moveDragToPoint:(CGPoint)location;
- (void)dragAutoScroll:(CADisplayLink *)link;
- (void)dragAutoScrollFor:(double)frame;
@end

/*
 * The auto scroll display link's target. It holds the list weakly. A display link retains its
 * target, and the list would otherwise stay alive until the drag ends.
 */
@interface ShadowListKitDragLinkTarget : NSObject
@property (nonatomic, weak) ShadowListKitListView *list;
@end

@implementation ShadowListKitDragLinkTarget

- (void)tick:(CADisplayLink *)link
{
  ShadowListKitListView *list = _list;
  if (!list) {
    [link invalidate];
    return;
  }
  [list dragAutoScroll:link];
}

@end

/*
 * Drag with the mouse to reorder. A press on an item that can move lifts it as soon as the mouse
 * drags it, without a hold. The held row follows the mouse, the other rows slide aside to open
 * the gap, and the list scrolls while the row is held near an edge. The drop slot and the shifts
 * come from the core's ListDriver, which runs the host layer's DragReorder.
 */
@implementation ShadowListKitListView (Drag)

- (BOOL)reorderEnabled
{
  return _reorderEnabled;
}

/*
 * Turning reorder off puts a held row back.
 */
- (void)setReorderEnabled:(BOOL)reorderEnabled
{
  _reorderEnabled = reorderEnabled;
  if (!reorderEnabled) {
    _dragPressed = NO;
    [self endDrag:NO];
  }
}

- (BOOL)hasHeldRow
{
  return _heldCell != nil;
}

/*
 * A point in the list's coordinates in the content the core places rows in.
 */
- (CGPoint)contentPointOf:(CGPoint)point
{
  CGPoint offset = self.contentOffset;
  return CGPointMake(point.x + offset.x, point.y + offset.y);
}

/*
 * A cell's layer is anchored at its corner. The transform scales it about its middle, then
 * moves it by x and y.
 */
- (CATransform3D)dragTransformOfCell:(ShadowListKitListCell *)cell x:(CGFloat)x y:(CGFloat)y scale:(CGFloat)scale
{
  CGSize size = cell.bounds.size;
  CGPoint anchor = cell.layer.anchorPoint;
  CGFloat moveX = x + size.width * (0.5 - anchor.x) * (1 - scale);
  CGFloat moveY = y + size.height * (0.5 - anchor.y) * (1 - scale);
  return CATransform3DConcat(CATransform3DMakeScale(scale, scale, 1), CATransform3DMakeTranslation(moveX, moveY, 0));
}

- (CATransform3D)dragTransformOfCell:(ShadowListKitListCell *)cell offset:(DragOffset)offset scale:(CGFloat)scale
{
  CGFloat along = (CGFloat)offset.leading;
  CGFloat cross = (CGFloat)offset.cross;
  return _horizontal ? [self dragTransformOfCell:cell x:along y:cross scale:scale]
                     : [self dragTransformOfCell:cell x:cross y:along scale:scale];
}

/*
 * The mouse in the list's coordinates, which are the viewport's. Auto scroll moves the content
 * under it.
 */
- (void)setDragTouch:(CGPoint)location
{
  _dragTouch = location;
  _hoverPoint = location;
}

#pragma mark - Pick up

/*
 * A plain press on an item that can move. The list keeps the press, which lifts the item once
 * the mouse drags it past the slop.
 */
- (BOOL)pressForDragWithEvent:(NSEvent *)event
{
  _dragPressed = NO;
  NSEventModifierFlags flags = event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
  NSEventModifierFlags modifiers = NSEventModifierFlagControl | NSEventModifierFlagCommand | NSEventModifierFlagShift;
  if (!_reorderEnabled || [self hasHeldRow] || event.clickCount > 1 || (flags & modifiers)) {
    return NO;
  }
  CGPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  if (![self movableCellAtPoint:point]) {
    return NO;
  }
  _dragPressed = YES;
  _dragStart = point;
  return YES;
}

/*
 * The held row follows the mouse. A press lifts its item once the mouse moved past the slop,
 * and the row then jumps to the mouse.
 */
- (void)mouseDragged:(NSEvent *)event
{
  CGPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  if ([self hasHeldRow]) {
    [self moveDragToPoint:point];
    return;
  }
  if (!_dragPressed) {
    [super mouseDragged:event];
    return;
  }
  if (std::hypot(point.x - _dragStart.x, point.y - _dragStart.y) <= SHADOWLIST_KIT_DRAG_SLOP) {
    return;
  }
  _dragPressed = NO;
  [self setDragTouch:_dragStart];
  [self beginDragAtPoint:_dragStart];
  if ([self hasHeldRow]) {
    [self moveDragToPoint:point];
  }
}

- (BOOL)releaseDragPress
{
  BOOL pressed = _dragPressed || [self hasHeldRow];
  _dragPressed = NO;
  [self endDrag:YES];
  return pressed;
}

/*
 * Pick up the row under a point, if it can move. Scrolling stops, the press highlight and the
 * hover go, and keys wait until the drop. Escape puts the row back.
 */
- (void)beginDragAtPoint:(CGPoint)location
{
  if (!_reorderEnabled || [self hasHeldRow]) {
    return;
  }
  ShadowListKitListCell *cell = [self movableCellAtPoint:location];
  if (!cell) {
    return;
  }
  [self stopScrolling];
  [self unhighlight];
  CGPoint content = [self contentPointOf:location];
  _driver.dragBegin((std::size_t)cell.row, [self along:content], [self cross:content]);
  _heldCell = cell;
  _dragFeedback.reset(_driver.getDragOriginIndex());
  [self updateHover];
  [self liftCell:cell];
  [self startDragLink];
  [self startDragKeyMonitor];
  [self updateDrag];
}

- (ShadowListKitListCell *)movableCellAtPoint:(CGPoint)location
{
  ShadowListKitListCell *cell = [self itemCellAtPoint:location];
  if (!cell || (std::size_t)cell.row >= _driver.getRowCount()) {
    return nil;
  }
  if ([_delegate respondsToSelector:@selector(listView:canMoveItemAtIndex:)] &&
      ![_delegate listView:self canMoveItemAtIndex:cell.index]) {
    return nil;
  }
  return cell;
}

- (void)liftCell:(ShadowListKitListCell *)cell
{
  CALayer *layer = cell.layer;
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [layer removeAnimationForKey:SHADOWLIST_KIT_TRANSFORM_KEY];
  layer.zPosition = 2;
  layer.shadowColor = NSColor.blackColor.CGColor;
  layer.shadowOpacity = (float)DRAG_LIFT_SHADOW_OPACITY;
  layer.shadowRadius = DRAG_LIFT_SHADOW_RADIUS;
  layer.shadowOffset = CGSizeMake(0, DRAG_LIFT_SHADOW_OFFSET);
  [CATransaction commit];
  ShadowListKitPerformHaptic(NSHapticFeedbackPatternGeneric);
}

- (void)startDragLink
{
  [_dragLink invalidate];
  ShadowListKitDragLinkTarget *target = [ShadowListKitDragLinkTarget new];
  target.list = self;
  _dragLink = [self displayLinkWithTarget:target selector:@selector(tick:)];
  [_dragLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

/*
 * The list may not be the first responder, which leaves Escape to another view. The monitor
 * sees every key of the app while a row is held.
 */
- (void)startDragKeyMonitor
{
  if (_dragKeyMonitor) {
    return;
  }
  __weak ShadowListKitListView *weakSelf = self;
  _dragKeyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent *(NSEvent *event) {
    ShadowListKitListView *list = weakSelf;
    if (![list hasHeldRow]) {
      return event;
    }
    if (event.keyCode == SHADOWLIST_KIT_ESCAPE_KEY_CODE) {
      [list cancelOperation:nil];
    }
    return nil;
  }];
}

- (void)stopDragKeyMonitor
{
  if (_dragKeyMonitor) {
    [NSEvent removeMonitor:_dragKeyMonitor];
    _dragKeyMonitor = nil;
  }
}

#pragma mark - Follow

- (void)moveDragToPoint:(CGPoint)location
{
  [self setDragTouch:location];
  [self updateDrag];
}

/*
 * Place the held row under the mouse and slide the others to open the gap. A new drop spot
 * ticks the trackpad.
 */
- (void)updateDrag
{
  if (![self hasHeldRow]) {
    return;
  }
  std::size_t held = _driver.getHeldIndex();
  if (held == UNDEFINED_INDEX) {
    [self endDrag:NO];
    return;
  }
  CGPoint touch = [self contentPointOf:_dragTouch];
  DragOffset placed = _driver.placeHeld(held, [self along:touch], [self cross:touch]);
  ShadowListKitSetTransform(_heldCell.layer, [self dragTransformOfCell:_heldCell offset:placed scale:SHADOWLIST_KIT_LIFT_SCALE], 0);
  std::size_t insertion = _driver.getDragInsertionIndex();
  _driver.dragUpdateInsertion([self mountedRows]);
  std::size_t next = _driver.getDragInsertionIndex();
  if (_dragFeedback.shouldTick(next, CACurrentMediaTime() * 1000.0)) {
    ShadowListKitPerformHaptic(NSHapticFeedbackPatternAlignment);
  }
  [self applyDragShiftsAnimated:next != insertion];
}

- (std::vector<std::size_t>)mountedRows
{
  std::vector<std::size_t> rows;
  rows.reserve(_mounted.size());
  for (auto& entry : _mounted) {
    ShadowListKitListCell *cell = entry.second;
    if (!cell.hidden && cell.mountGeneration != 0 && cell.row != NSNotFound) {
      rows.push_back((std::size_t)cell.row);
    }
  }
  return rows;
}

/*
 * Slide every mounted row except the held one by its shift.
 */
- (void)applyDragShiftsAnimated:(BOOL)animated
{
  for (auto& entry : _mounted) {
    ShadowListKitListCell *cell = entry.second;
    if (cell == _heldCell || cell.hidden || cell.row == NSNotFound) {
      continue;
    }
    DragOffset shift = _driver.dragShiftFor((std::size_t)cell.row);
    CATransform3D transform = [self dragTransformOfCell:cell offset:shift scale:1];
    if (!CATransform3DEqualToTransform(cell.layer.transform, transform)) {
      ShadowListKitSetTransform(cell.layer, transform, animated ? SHADOWLIST_KIT_SHIFT_DURATION : 0);
    }
  }
}

- (void)dragAutoScroll:(CADisplayLink *)link
{
  [self dragAutoScrollFor:link.targetTimestamp - link.timestamp];
}

/*
 * Scroll while the row is held near an edge, by a frame of the given seconds. The move counts as
 * the user's scrolling, the same as the wheel moving the list. A driven list asks its host
 * through listView:didCorrectOffset:. The layout pass after it places the held row again.
 */
- (void)dragAutoScrollFor:(double)frame
{
  if (![self hasHeldRow]) {
    return;
  }
  double touch = [self along:_dragTouch] - [self leadingInset];
  double offset = [self offset];
  double next = dragAutoScrollOffset(DRAG_AUTO_SCROLL_IOS, touch, _windowAlong, offset, [self maxOffset], frame);
  if (std::fabs(next - offset) >= 0.5) {
    [self writeOffset:next byUser:YES];
    [self layoutSubtreeIfNeeded];
  }
}

#pragma mark - Drop

/*
 * Let go of the row. With commit the data moves, and the row settles from where it was let go
 * into its new slot. Without it the rows slide back.
 */
- (void)endDrag:(BOOL)commit
{
  if (![self hasHeldRow]) {
    return;
  }
  [_dragLink invalidate];
  _dragLink = nil;
  [self stopDragKeyMonitor];
  ShadowListKitListCell *cell = _heldCell;
  std::size_t from = _driver.getDragOriginIndex();
  std::size_t to = _driver.getDragInsertionIndex();
  _heldCell = nil;
  _driver.dragEnd();
  NSInteger fromItem = NSNotFound;
  NSInteger toItem = NSNotFound;
  BOOL moves = commit && from != UNDEFINED_INDEX && to != UNDEFINED_INDEX && from != to &&
    [self itemsForDragFromRow:from toRow:to from:&fromItem to:&toItem] &&
    [_delegate respondsToSelector:@selector(listView:moveItemAtIndex:toIndex:)];
  if (moves) {
    [self commitMoveOfCell:cell from:fromItem to:toItem];
  }
  [self dropCell:cell];
  [self updateHover];
}

/*
 * Move the row in the data and lay out again. The held cell keeps showing where it was let go.
 */
- (void)commitMoveOfCell:(ShadowListKitListCell *)cell from:(NSInteger)from to:(NSInteger)to
{
  CGRect shown = cell.layer.frame;
  [_delegate listView:self moveItemAtIndex:from toIndex:to];
  for (auto& entry : _mounted) {
    ShadowListKitSetTransform(entry.second.layer, CATransform3DIdentity, 0);
  }
  [self reloadData];
  [self layoutSubtreeIfNeeded];
  CGRect resting = cell.layer.frame;
  CATransform3D transform = [self dragTransformOfCell:cell x:CGRectGetMidX(shown) - CGRectGetMidX(resting)
                                                    y:CGRectGetMidY(shown) - CGRectGetMidY(resting)
                                                scale:SHADOWLIST_KIT_LIFT_SCALE];
  ShadowListKitSetTransform(cell.layer, transform, 0);
}

/*
 * Every row slides home over the drop's duration. The dropped row loses its shadow once there,
 * unless it was picked up again.
 */
- (void)dropCell:(ShadowListKitListCell *)cell
{
  __weak ShadowListKitListView *weakSelf = self;
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    ShadowListKitListView *list = weakSelf;
    if (list && list->_heldCell != cell) {
      [list resetDragLookOfCell:cell];
    }
  }];
  for (auto& entry : _mounted) {
    if (!CATransform3DIsIdentity(entry.second.layer.transform)) {
      ShadowListKitSetTransform(entry.second.layer, CATransform3DIdentity, SHADOWLIST_KIT_DROP_DURATION);
    }
  }
  ShadowListKitSetTransform(cell.layer, CATransform3DIdentity, SHADOWLIST_KIT_DROP_DURATION);
  [CATransaction commit];
}

- (void)resetDragLookOfCell:(ShadowListKitListCell *)cell
{
  CALayer *layer = cell.layer;
  if (CATransform3DIsIdentity(layer.transform) && layer.shadowOpacity == 0 && layer.zPosition == 0) {
    return;
  }
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [layer removeAnimationForKey:SHADOWLIST_KIT_TRANSFORM_KEY];
  layer.transform = CATransform3DIdentity;
  layer.zPosition = 0;
  layer.shadowOpacity = 0;
  [CATransaction commit];
}

@end

#pragma mark - Testing

@implementation ShadowListKitListView (DragTesting)

- (void)shadowListKit_beginDragAtPoint:(CGPoint)point
{
  [self setDragTouch:point];
  [self beginDragAtPoint:point];
}

- (void)shadowListKit_moveDragToPoint:(CGPoint)point
{
  [self moveDragToPoint:point];
}

- (void)shadowListKit_endDrag
{
  [self endDrag:YES];
}

- (void)shadowListKit_mouseDownAtPoint:(CGPoint)point
{
  [self shadowListKit_sendDragMouse:NSEventTypeLeftMouseDown at:point];
}

- (void)shadowListKit_mouseDragToPoint:(CGPoint)point
{
  [self shadowListKit_sendDragMouse:NSEventTypeLeftMouseDragged at:point];
}

- (void)shadowListKit_mouseUpAtPoint:(CGPoint)point
{
  [self shadowListKit_sendDragMouse:NSEventTypeLeftMouseUp at:point];
}

/*
 * Auto scroll frames of 1/120 s, the way the display link's frames each scroll.
 */
- (void)shadowListKit_runDragFramesFor:(NSTimeInterval)duration
{
  NSTimeInterval frame = 1.0 / 120;
  for (NSTimeInterval run = 0; run < duration; run += frame) {
    [self dragAutoScrollFor:frame];
  }
}

- (NSInteger)shadowListKit_heldIndex
{
  std::size_t held = [self hasHeldRow] ? _driver.getHeldIndex() : UNDEFINED_INDEX;
  return held == UNDEFINED_INDEX ? NSNotFound : [self itemForRow:(NSInteger)held];
}

/*
 * The item the held row would drop at: its own while it would stay.
 */
- (NSInteger)shadowListKit_dropIndex
{
  std::size_t from = _driver.getDragOriginIndex();
  std::size_t to = _driver.getDragInsertionIndex();
  if (![self hasHeldRow] || from == UNDEFINED_INDEX) {
    return NSNotFound;
  }
  NSInteger fromItem = NSNotFound;
  NSInteger toItem = NSNotFound;
  if (to != UNDEFINED_INDEX && [self itemsForDragFromRow:from toRow:to from:&fromItem to:&toItem]) {
    return toItem;
  }
  return [self itemForRow:(NSInteger)from];
}

- (void)shadowListKit_sendDragMouse:(NSEventType)type at:(CGPoint)point
{
  NSPoint window = [self convertPoint:point toView:nil];
  NSEvent *event = [NSEvent mouseEventWithType:type location:window modifierFlags:0
                                     timestamp:NSProcessInfo.processInfo.systemUptime
                                  windowNumber:self.window.windowNumber context:nil eventNumber:0 clickCount:1
                                      pressure:type == NSEventTypeLeftMouseUp ? 0 : 1];
  switch (type) {
    case NSEventTypeLeftMouseDown:
      [self mouseDown:event];
      break;
    case NSEventTypeLeftMouseDragged:
      [self mouseDragged:event];
      break;
    default:
      [self mouseUp:event];
      break;
  }
  [self layoutSubtreeIfNeeded];
}

@end
