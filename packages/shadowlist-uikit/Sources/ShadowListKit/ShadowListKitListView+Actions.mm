#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitSwipeActionsView.h"

#include <cmath>

using namespace azimgd::shadowlist;

/*
 * How long a released row takes to rest, the core's SWIPE_SETTLE_DURATION_MS.
 */
static const NSTimeInterval SHADOWLIST_KIT_SWIPE_DURATION = SWIPE_SETTLE_DURATION_MS / 1000.0;

#pragma mark - Swipe and menus

/*
 * Swipe actions and context menus. A pan across the scroll axis on a row with actions moves
 * the row and shows its buttons. SwipeReveal in the host layer decides how far it follows and
 * where it rests. Touching and holding a row shows its menu through UIContextMenuInteraction,
 * unless the row can be reordered. The hold then lifts the row, and the menu shows when it is let
 * go in place, through UIEditMenuInteraction.
 */
@implementation ShadowListKitListView (Actions)

- (void)installActionGestures
{
  if (!_swipePan) {
    _swipePan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handleSwipePan:)];
    /*
     * The list decides in gestureRecognizerShouldBegin whether a pan swipes a row. UIKit asks
     * it only through the delegate. Without one the pan began on vertical drags too and the
     * scroll view never scrolled.
     */
    _swipePan.delegate = (id<UIGestureRecognizerDelegate>)self;
    [self addGestureRecognizer:_swipePan];
  }
  BOOL menus = [_userDelegate respondsToSelector:@selector(listView:contextMenuForItemAtIndex:)];
  if (menus && !_menuInteraction) {
    _menuInteraction = [[UIContextMenuInteraction alloc] initWithDelegate:self];
    [self addInteraction:_menuInteraction];
    _editMenu = [[UIEditMenuInteraction alloc] initWithDelegate:self];
    [self addInteraction:_editMenu];
  } else if (!menus && _menuInteraction) {
    [self removeInteraction:_menuInteraction];
    [self removeInteraction:_editMenu];
    _menuInteraction = nil;
    _editMenu = nil;
  }
}

- (BOOL)isSwipeOpen
{
  return _swipeCell != nil;
}

- (BOOL)isSwipedOutCell:(ShadowListKitListCell *)cell
{
  return cell == _swipeCell && _swipe.isSwipedOut(_swipeOffset);
}

#pragma mark - Swipe gesture

- (ShadowListKitSwipeActionsConfiguration *)leadingActionsForCell:(ShadowListKitListCell *)cell
{
  if (![_userDelegate respondsToSelector:@selector(listView:leadingSwipeActionsForItemAtIndex:)]) {
    return nil;
  }
  ShadowListKitSwipeActionsConfiguration *configuration = [_userDelegate listView:self leadingSwipeActionsForItemAtIndex:cell.index];
  return configuration.actions.count > 0 ? configuration : nil;
}

- (ShadowListKitSwipeActionsConfiguration *)trailingActionsForCell:(ShadowListKitListCell *)cell
{
  if (![_userDelegate respondsToSelector:@selector(listView:trailingSwipeActionsForItemAtIndex:)]) {
    return nil;
  }
  ShadowListKitSwipeActionsConfiguration *configuration = [_userDelegate listView:self trailingSwipeActionsForItemAtIndex:cell.index];
  return configuration.actions.count > 0 ? configuration : nil;
}

/*
 * The row a pan across the axis would swipe, or nil. A pan along the axis scrolls instead.
 */
- (ShadowListKitListCell *)swipeCellForPan:(UIPanGestureRecognizer *)pan
{
  if (_editing || [self hasHeldRow]) {
    return nil;
  }
  CGPoint velocity = [pan velocityInView:self];
  CGFloat cross = [self cross:velocity];
  if (std::fabs(cross) <= std::fabs([self along:velocity])) {
    return nil;
  }
  ShadowListKitListCell *cell = [self itemCellAtPoint:[pan locationInView:self]];
  if (!cell) {
    return nil;
  }
  if (cell == _swipeCell) {
    return cell;
  }
  BOOL revealsLeading = cross > 0;
  return (revealsLeading ? [self leadingActionsForCell:cell] : [self trailingActionsForCell:cell]) ? cell : nil;
}

- (BOOL)shouldBeginActionGesture:(UIGestureRecognizer *)gesture
{
  return gesture == _swipePan && [self swipeCellForPan:_swipePan] != nil;
}

- (BOOL)swipeTakesPan:(UIPanGestureRecognizer *)pan
{
  return [self swipeCellForPan:pan] != nil;
}

- (void)handleSwipePan:(UIPanGestureRecognizer *)pan
{
  switch (pan.state) {
    case UIGestureRecognizerStateBegan: {
      ShadowListKitListCell *cell = [self swipeCellForPan:pan] ?: [self itemCellAtPoint:[pan locationInView:self]];
      if (cell != _swipeCell) {
        [self closeSwipeAnimated:NO];
        [self openActionsForCell:cell];
      }
      if (!_swipeCell) {
        return;
      }
      [self unhighlightCellsForSwipe];
      _swipe.begin(_swipe.getSpec(), _swipeOffset);
      break;
    }
    case UIGestureRecognizerStateChanged: {
      if (!_swipeCell) {
        return;
      }
      BOOL wasPast = _swipe.isPastFullSwipe(_swipeOffset);
      [self setSwipeOffset:(CGFloat)_swipe.drag([self cross:[pan translationInView:self]])];
      if (_swipe.isPastFullSwipe(_swipeOffset) != wasPast) {
        [[UIImpactFeedbackGenerator new] impactOccurred];
      }
      break;
    }
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed: {
      if (!_swipeCell) {
        return;
      }
      SwipeRest rest = _swipe.settle(_swipeOffset, [self cross:[pan velocityInView:self]], SWIPE_FLING_VELOCITY);
      [self settleSwipeTo:rest];
      break;
    }
    default:
      break;
  }
}

- (void)scriptSwipeOfCell:(ShadowListKitListCell *)cell distance:(CGFloat)distance velocity:(CGFloat)velocity
{
  if (cell != _swipeCell) {
    [self closeSwipeAnimated:NO];
    [self openActionsForCell:cell];
  }
  if (!_swipeCell) {
    return;
  }
  _swipe.begin(_swipe.getSpec(), _swipeOffset);
  [self setSwipeOffset:(CGFloat)_swipe.drag(distance)];
  [self settleSwipeTo:_swipe.settle(_swipeOffset, velocity, SWIPE_FLING_VELOCITY)];
}

- (void)unhighlightCellsForSwipe
{
  if (_swipeCell.isHighlighted) {
    [_swipeCell setHighlighted:NO animated:NO];
  }
}

/*
 * Put the actions of a row under it, closed.
 */
- (void)openActionsForCell:(ShadowListKitListCell *)cell
{
  if (!cell) {
    return;
  }
  ShadowListKitSwipeActionsConfiguration *leading = [self leadingActionsForCell:cell];
  ShadowListKitSwipeActionsConfiguration *trailing = [self trailingActionsForCell:cell];
  if (!leading && !trailing) {
    return;
  }
  ShadowListKitSwipeActionsView *view = [[ShadowListKitSwipeActionsView alloc] initWithLeading:leading trailing:trailing horizontal:_horizontal];
  __weak ShadowListKitListView *weakSelf = self;
  view.onAction = ^(ShadowListKitSwipeAction *action) {
    [weakSelf performSwipeAction:action];
  };
  view.bounds = cell.bounds;
  view.center = cell.center;
  [self insertSubview:view belowSubview:cell];
  SwipeSpec spec;
  spec.leadingWidth = [view leadingWidth];
  spec.trailingWidth = [view trailingWidth];
  spec.leadingFullSwipe = leading.performsFirstActionWithFullSwipe;
  spec.trailingFullSwipe = trailing.performsFirstActionWithFullSwipe;
  spec.rowSize = _horizontal ? cell.bounds.size.height : cell.bounds.size.width;
  _swipe.begin(spec, 0);
  _swipeCell = cell;
  _swipeActionsView = view;
  _swipeOffset = 0;
}

- (void)setSwipeOffset:(CGFloat)offset
{
  _swipeOffset = offset;
  _swipeCell.transform = _horizontal ? CGAffineTransformMakeTranslation(0, offset)
                                     : CGAffineTransformMakeTranslation(offset, 0);
  if (offset != 0) {
    [_swipeActionsView layoutForOffset:offset full:_swipe.isPastFullSwipe(offset)];
  }
}

- (void)settleSwipeTo:(SwipeRest)rest
{
  ShadowListKitListCell *cell = _swipeCell;
  [UIView animateWithDuration:SHADOWLIST_KIT_SWIPE_DURATION delay:0 usingSpringWithDamping:1 initialSpringVelocity:0
                      options:UIViewAnimationOptionBeginFromCurrentState | UIViewAnimationOptionAllowUserInteraction
                   animations:^{
                     [self setSwipeOffset:(CGFloat)rest.offset];
                     [self->_swipeActionsView layoutIfNeeded];
                   }
                   completion:^(BOOL) {
                     if (rest.side == SwipeSide::None && cell == self->_swipeCell && self->_swipeOffset == 0) {
                       [self tearDownSwipe];
                     }
                   }];
  if (rest.full) {
    ShadowListKitSwipeActionsConfiguration *configuration = rest.side == SwipeSide::Leading ? _swipeActionsView.leading
                                                                                    : _swipeActionsView.trailing;
    [self performSwipeAction:configuration.actions.firstObject];
  }
}

/*
 * Run an action. Its completion closes the row, unless the action removed it.
 */
- (void)performSwipeAction:(ShadowListKitSwipeAction *)action
{
  ShadowListKitListCell *cell = _swipeCell;
  if (!action || !cell) {
    return;
  }
  if (!action.handler) {
    [self closeSwipeAnimated:YES];
    return;
  }
  __weak ShadowListKitListView *weakSelf = self;
  action.handler(action, ^(BOOL) {
    dispatch_async(dispatch_get_main_queue(), ^{
      ShadowListKitListView *list = weakSelf;
      if (list && list->_swipeCell == cell) {
        [list closeSwipeAnimated:YES];
      }
    });
  });
}

- (void)closeSwipeAnimated:(BOOL)animated
{
  if (!_swipeCell) {
    return;
  }
  if (!animated) {
    [self setSwipeOffset:0];
    [self tearDownSwipe];
    return;
  }
  SwipeRest closed;
  [self settleSwipeTo:closed];
}

- (void)tearDownSwipe
{
  [_swipeActionsView removeFromSuperview];
  _swipeActionsView = nil;
  if (_swipeCell && !CGAffineTransformIsIdentity(_swipeCell.transform) && _swipeOffset != 0) {
    _swipeCell.transform = CGAffineTransformIdentity;
  }
  _swipeCell = nil;
  _swipeOffset = 0;
}

- (void)swipeCellWillRecycle:(ShadowListKitListCell *)cell
{
  if (cell == _swipeCell) {
    [self tearDownSwipe];
  }
}

/*
 * Keep the actions under the swiped row after a layout pass moved it. A row that left the
 * screen closes.
 */
- (void)layoutSwipe
{
  if (!_swipeCell) {
    return;
  }
  if (_swipeCell.hidden || _swipeCell.row == NSNotFound) {
    [self tearDownSwipe];
    return;
  }
  _swipeActionsView.bounds = _swipeCell.bounds;
  _swipeActionsView.center = _swipeCell.center;
}

/*
 * A tap while a row is open closes it, unless it hits an action button.
 */
- (BOOL)swipeOwnsTapAtPoint:(CGPoint)point
{
  if (!_swipeCell) {
    return NO;
  }
  CGPoint local = [self convertPoint:point toView:_swipeActionsView];
  if (![_swipeActionsView hasButtonAtPoint:local]) {
    [self closeSwipeAnimated:YES];
  }
  return YES;
}

#pragma mark - Menus

- (UIMenu *)menuForCell:(ShadowListKitListCell *)cell
{
  if (!cell || cell.index == NSNotFound || ![_userDelegate respondsToSelector:@selector(listView:contextMenuForItemAtIndex:)]) {
    return nil;
  }
  return [_userDelegate listView:self contextMenuForItemAtIndex:cell.index];
}

- (BOOL)canMoveCell:(ShadowListKitListCell *)cell
{
  if (!self.reorderEnabled || _editing) {
    return NO;
  }
  return ![_userDelegate respondsToSelector:@selector(listView:canMoveItemAtIndex:)] ||
    [_userDelegate listView:self canMoveItemAtIndex:cell.index];
}

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction
                        configurationForMenuAtLocation:(CGPoint)location
{
  ShadowListKitListCell *cell = [self itemCellAtPoint:location];
  // A row that can be reordered lifts on the hold. Its menu waits for the release.
  if (!cell || [self hasHeldRow] || [self isSwipeOpen] || [self canMoveCell:cell]) {
    return nil;
  }
  UIMenu *menu = [self menuForCell:cell];
  // A data change since the last layout pass may have dropped the cell's row.
  if (!menu || (std::size_t)cell.row >= _driver.getKeyCount()) {
    return nil;
  }
  NSString *key = ShadowListKitString(_driver.getKeyAt((std::size_t)cell.row));
  return [UIContextMenuConfiguration configurationWithIdentifier:key previewProvider:nil
                                                  actionProvider:^UIMenu *(NSArray<UIMenuElement *> *) {
                                                    return menu;
                                                  }];
}

- (UITargetedPreview *)previewForConfiguration:(UIContextMenuConfiguration *)configuration
{
  NSString *key = (NSString *)configuration.identifier;
  auto mounted = _mounted.find(ShadowListKitStdString(key));
  if (mounted == _mounted.end() || mounted->second.hidden || !mounted->second.window) {
    return nil;
  }
  return [[UITargetedPreview alloc] initWithView:mounted->second];
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction
                                 configuration:(UIContextMenuConfiguration *)configuration
         highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier
{
  return [self previewForConfiguration:configuration];
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction
                                 configuration:(UIContextMenuConfiguration *)configuration
           dismissalPreviewForItemWithIdentifier:(id<NSCopying>)identifier
{
  return [self previewForConfiguration:configuration];
}

- (BOOL)showMenuForCell:(ShadowListKitListCell *)cell atPoint:(CGPoint)point
{
  UIMenu *menu = [self menuForCell:cell];
  if (!menu || !_editMenu) {
    return NO;
  }
  _pendingMenu = menu;
  UIEditMenuConfiguration *configuration = [UIEditMenuConfiguration configurationWithIdentifier:nil sourcePoint:point];
  [_editMenu presentEditMenuWithConfiguration:configuration];
  return YES;
}

- (UIMenu *)editMenuInteraction:(UIEditMenuInteraction *)interaction
           menuForConfiguration:(UIEditMenuConfiguration *)configuration
               suggestedActions:(NSArray<UIMenuElement *> *)suggestedActions
{
  UIMenu *menu = _pendingMenu;
  _pendingMenu = nil;
  return menu;
}

@end
