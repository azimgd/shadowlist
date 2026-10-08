#import "Internal/SLKListView+Private.h"
#import "SLKListView+Testing.h"

#include <cmath>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * Scale of a held row.
 */
static const CGFloat SLK_LIFT_SCALE = 1.03;
static const NSTimeInterval SLK_LIFT_DURATION = 0.2;
static const NSTimeInterval SLK_SHIFT_DURATION = 0.22;
static const NSTimeInterval SLK_DROP_DURATION = 0.25;
static const NSTimeInterval SLK_PRESS_DURATION = 0.35;

/*
 * How far a held row may move and still count as held in place, which shows its menu on release.
 */
static const CGFloat SLK_MENU_SLOP = 10;

@interface SLKListView (DragSteps)
- (void)setDragTouch:(CGPoint)location;
- (void)beginDragAtPoint:(CGPoint)location;
- (void)updateDrag;
- (void)endDrag:(BOOL)commit;
@end

/*
 * Touch and hold to reorder. The held row follows the finger, the other rows slide aside to
 * open the gap, and the list scrolls while the row is held near an edge. The drop slot and
 * the shifts come from the core's ListDriver, which runs the host layer's DragReorder.
 */
@implementation SLKListView (Drag)

- (void)enableDragPress:(BOOL)enabled
{
  if (enabled && !_dragPress) {
    _dragPress = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleDragPress:)];
    _dragPress.minimumPressDuration = SLK_PRESS_DURATION;
    [self addGestureRecognizer:_dragPress];
  }
  _dragPress.enabled = enabled;
}

- (BOOL)hasHeldRow
{
  return _heldCell != nil;
}

- (CGAffineTransform)transformAlong:(double)along cross:(double)cross
{
  return _horizontal ? CGAffineTransformMakeTranslation((CGFloat)along, (CGFloat)cross)
                     : CGAffineTransformMakeTranslation((CGFloat)cross, (CGFloat)along);
}

/*
 * Keep the finger in viewport coordinates. Auto scroll moves the content under it.
 */
- (void)setDragTouch:(CGPoint)location
{
  CGPoint offset = self.contentOffset;
  _dragTouch = CGPointMake(location.x - offset.x, location.y - offset.y);
}

- (void)handleDragPress:(UILongPressGestureRecognizer *)press
{
  CGPoint location = [press locationInView:self];
  [self setDragTouch:location];
  switch (press.state) {
    case UIGestureRecognizerStateBegan:
      _dragStart = location;
      _dragMoved = NO;
      [self beginDragAtPoint:location];
      break;
    case UIGestureRecognizerStateChanged:
      if (std::hypot(location.x - _dragStart.x, location.y - _dragStart.y) > SLK_MENU_SLOP) {
        _dragMoved = YES;
      }
      [self updateDrag];
      break;
    case UIGestureRecognizerStateEnded:
      [self endDrag:YES];
      break;
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
      [self endDrag:NO];
      break;
    default:
      break;
  }
}

#pragma mark - Pick up

/*
 * Pick up the row under a point, if it can move.
 */
- (void)beginDragAtPoint:(CGPoint)location
{
  SLKListCell *cell = [self movableCellAtPoint:location];
  if (!cell) {
    return;
  }
  [self closeSwipeAnimated:YES];
  _driver.dragBegin((std::size_t)cell.row, [self along:location], [self cross:location]);
  _heldCell = cell;
  [self liftCell:cell];
  _dragLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(dragAutoScroll:)];
  [_dragLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
  [UIView animateWithDuration:SLK_LIFT_DURATION animations:^{
    [self updateDrag];
  }];
}

- (SLKListCell *)movableCellAtPoint:(CGPoint)location
{
  SLKListCell *cell = [self itemCellAtPoint:location];
  if (!cell || (std::size_t)cell.row >= _driver.getCount() || _editing) {
    return nil;
  }
  if ([_userDelegate respondsToSelector:@selector(listView:canMoveItemAtIndex:)] &&
      ![_userDelegate listView:self canMoveItemAtIndex:cell.index]) {
    return nil;
  }
  return cell;
}

- (void)liftCell:(SLKListCell *)cell
{
  cell.layer.zPosition = 2;
  cell.layer.shadowColor = UIColor.blackColor.CGColor;
  cell.layer.shadowOpacity = 0.18f;
  cell.layer.shadowRadius = 12;
  cell.layer.shadowOffset = CGSizeMake(0, 4);
  [[UIImpactFeedbackGenerator new] impactOccurred];
}

#pragma mark - Follow

/*
 * Place the held row under the finger and slide the others to open the gap.
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
  CGPoint offset = self.contentOffset;
  CGPoint touch = CGPointMake(_dragTouch.x + offset.x, _dragTouch.y + offset.y);
  DragOffset placed = _driver.placeHeld(held, [self along:touch], [self cross:touch]);
  CGAffineTransform lift = CGAffineTransformMakeScale(SLK_LIFT_SCALE, SLK_LIFT_SCALE);
  _heldCell.transform = CGAffineTransformConcat(lift, [self transformAlong:placed.leading cross:placed.cross]);
  std::size_t insertion = _driver.getDragInsertionIndex();
  _driver.dragUpdateInsertion([self mountedIndices]);
  [self applyDragShiftsAnimated:_driver.getDragInsertionIndex() != insertion];
}

- (std::vector<std::size_t>)mountedIndices
{
  std::vector<std::size_t> indices;
  indices.reserve(_mounted.size());
  for (auto& entry : _mounted) {
    if (!entry.second.hidden && entry.second.row != NSNotFound) {
      indices.push_back((std::size_t)entry.second.row);
    }
  }
  return indices;
}

/*
 * Slide every mounted row except the held one by its shift.
 */
- (void)applyDragShiftsAnimated:(BOOL)animated
{
  void (^apply)(void) = ^{
    for (auto& entry : self->_mounted) {
      SLKListCell *cell = entry.second;
      if (cell == self->_heldCell || cell.hidden || cell.row == NSNotFound) {
        continue;
      }
      DragOffset shift = self->_driver.dragShiftFor((std::size_t)cell.row);
      CGAffineTransform transform = [self transformAlong:shift.leading cross:shift.cross];
      if (!CGAffineTransformEqualToTransform(cell.transform, transform)) {
        cell.transform = transform;
      }
    }
  };
  if (animated) {
    [UIView animateWithDuration:SLK_SHIFT_DURATION delay:0
                        options:UIViewAnimationOptionBeginFromCurrentState | UIViewAnimationOptionAllowUserInteraction
                     animations:apply completion:nil];
  } else {
    apply();
  }
}

/*
 * Scroll while the row is held near an edge. The move counts as the user's scrolling, the
 * same as a finger moving the list.
 */
- (void)dragAutoScroll:(CADisplayLink *)link
{
  double touch = [self along:_dragTouch] - [self leadingInset];
  double offset = [self offset];
  double next = dragAutoScrollOffset(DRAG_AUTO_SCROLL_IOS, touch, _windowAlong, offset, [self maxOffset]);
  if (std::fabs(next - offset) >= 0.5) {
    [self writeOffset:next byUser:YES];
    [self updateDrag];
  }
}

#pragma mark - Drop

/*
 * Let go of the row. With commit the data moves, and the row flies from where it was let
 * go into its new slot.
 */
- (void)endDrag:(BOOL)commit
{
  if (![self hasHeldRow]) {
    return;
  }
  [_dragLink invalidate];
  _dragLink = nil;
  SLKListCell *cell = _heldCell;
  std::size_t from = _driver.getDragOriginIndex();
  std::size_t to = _driver.getDragInsertionIndex();
  _heldCell = nil;
  _driver.dragEnd();
  NSInteger fromItem = NSNotFound;
  NSInteger toItem = NSNotFound;
  BOOL moves = commit && from != UNDEFINED_INDEX && to != UNDEFINED_INDEX && from != to &&
    [self itemsForDragFromRow:from toRow:to from:&fromItem to:&toItem] &&
    [_userDelegate respondsToSelector:@selector(listView:moveItemAtIndex:toIndex:)];
  if (moves) {
    [self commitMoveOfCell:cell from:fromItem to:toItem];
  }
  [self dropCell:cell];
  // A row lifted and let go in place shows its menu.
  if (commit && !moves && !_dragMoved) {
    [self showMenuForCell:cell atPoint:_dragStart];
  }
}

/*
 * Move the row in the data and lay out again. The held cell keeps showing where it was let go.
 */
- (void)commitMoveOfCell:(SLKListCell *)cell from:(NSInteger)from to:(NSInteger)to
{
  CGPoint shown = CGPointMake(cell.center.x + cell.transform.tx, cell.center.y + cell.transform.ty);
  [_userDelegate listView:self moveItemAtIndex:from toIndex:to];
  for (auto& entry : _mounted) {
    entry.second.transform = CGAffineTransformIdentity;
  }
  // The drop animates the rows itself.
  BOOL animates = _animatesChanges;
  _animatesChanges = NO;
  [self reloadData];
  [self layoutIfNeeded];
  _animatesChanges = animates;
  cell.transform = CGAffineTransformConcat(CGAffineTransformMakeScale(SLK_LIFT_SCALE, SLK_LIFT_SCALE),
    CGAffineTransformMakeTranslation(shown.x - cell.center.x, shown.y - cell.center.y));
}

- (void)dropCell:(SLKListCell *)cell
{
  [UIView animateWithDuration:SLK_DROP_DURATION delay:0 usingSpringWithDamping:0.9 initialSpringVelocity:0
                      options:UIViewAnimationOptionBeginFromCurrentState animations:^{
                        for (auto& entry : self->_mounted) {
                          entry.second.transform = CGAffineTransformIdentity;
                        }
                        cell.transform = CGAffineTransformIdentity;
                      }
                   completion:^(BOOL finished) {
                     cell.layer.zPosition = 0;
                     cell.layer.shadowOpacity = 0;
                   }];
}

@end

@implementation SLKListView (Testing)

- (void)slk_beginDragAtPoint:(CGPoint)point
{
  [self setDragTouch:point];
  [self beginDragAtPoint:point];
}

- (void)slk_moveDragToPoint:(CGPoint)point
{
  [self setDragTouch:point];
  [self updateDrag];
}

- (void)slk_endDrag
{
  [self endDrag:YES];
}

- (void)slk_swipeItemAtIndex:(NSInteger)index distance:(CGFloat)distance velocity:(CGFloat)velocity
{
  SLKListCell *cell = [self cellForItemAtIndex:index];
  if (cell) {
    [self scriptSwipeOfCell:cell distance:distance velocity:velocity];
  }
}

@end
