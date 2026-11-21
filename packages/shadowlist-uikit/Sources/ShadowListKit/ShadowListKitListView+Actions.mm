#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitSwipeController.h"
#import "Internal/ShadowListKitMenuController.h"
#import <ShadowListKit/ShadowListKitListView+Testing.h>

using namespace azimgd::shadowlist;

extern "C" const char ShadowListKitListViewActionsLink = 0;

@implementation ShadowListKitListView (Actions)

- (void)installActionGestures
{
  if (!_swipeController) {
    _swipeController = [[ShadowListKitSwipeController alloc] initWithHost:self];
    _swipePan = _swipeController.pan;
  }
  if (!_menuController) _menuController = [[ShadowListKitMenuController alloc] initWithHost:self];
  [_menuController refresh];
  BOOL longPresses = [_userDelegate respondsToSelector:@selector(listView:didLongPressItemAtIndex:)];
  if (longPresses && !_longPress) {
    _longPress = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleLongPress:)];
    _longPress.delegate = (id<UIGestureRecognizerDelegate>)self;
    [self addGestureRecognizer:_longPress];
  } else if (!longPresses && _longPress) {
    [self removeGestureRecognizer:_longPress];
    _longPress = nil;
  }
}

- (BOOL)isSwipeOpen { return _swipeController.isOpen; }
- (ShadowListKitListCell *)swipeCell { return (id)_swipeController.content; }
- (BOOL)isSwipedOutCell:(ShadowListKitListCell *)cell { return [_swipeController isSwipedOutContent:cell]; }
- (void)closeSwipeAnimated:(BOOL)animated { [_swipeController closeAnimated:animated]; }
- (void)swipeCellWillRecycle:(ShadowListKitListCell *)cell { [_swipeController contentWillRecycle:cell]; }
- (void)layoutSwipe { [_swipeController layout]; }
- (BOOL)swipeOwnsTapAtPoint:(CGPoint)point { return [_swipeController ownsTapAtPoint:point]; }
- (BOOL)swipeTakesPan:(UIPanGestureRecognizer *)pan { return [_swipeController takesPan:pan]; }
- (void)scriptSwipeOfCell:(ShadowListKitListCell *)cell distance:(CGFloat)distance velocity:(CGFloat)velocity
{
  [_swipeController scriptSwipeOfContent:cell distance:distance velocity:velocity];
}

- (BOOL)shouldBeginActionGesture:(UIGestureRecognizer *)gesture
{
  if (gesture == _longPress) return [self longPressCellAtPoint:[gesture locationInView:self]] != nil;
  return gesture == _swipePan && [_swipeController takesPan:_swipePan];
}

- (BOOL)isCurrentActionCell:(UIView *)content
{
  if (![content isKindOfClass:ShadowListKitListCell.class]) return NO;
  ShadowListKitListCell *cell = (id)content;
  if (!self.window || cell.superview != self || cell.hidden || cell.row == NSNotFound || cell.index == NSNotFound ||
      (std::size_t)cell.row >= _driver.getKeyCount() || _sections.itemForRow((std::size_t)cell.row) != (std::size_t)cell.index) return NO;
  auto found = _mounted.find(_driver.getKeyAt((std::size_t)cell.row));
  return found != _mounted.end() && found->second == cell;
}

#pragma mark - Swipe host

- (UIView *)swipeRootView { return self; }
- (UIView *)swipeContentAtPoint:(CGPoint)point
{
  return _editing || [self hasHeldRow] ? nil : [self itemCellAtPoint:point];
}
- (BOOL)isSwipeContentValid:(UIView *)content { return !_editing && ![self hasHeldRow] && [self isCurrentActionCell:content]; }

- (ShadowListKitSwipeActionsConfiguration *)leadingActionsForContent:(UIView *)content
{
  if (![self isSwipeContentValid:content] || ![_userDelegate respondsToSelector:@selector(listView:leadingSwipeActionsForItemAtIndex:)]) return nil;
  ShadowListKitSwipeActionsConfiguration *configuration = [_userDelegate listView:self leadingSwipeActionsForItemAtIndex:((ShadowListKitListCell *)content).index];
  return configuration.actions.count ? configuration : nil;
}

- (ShadowListKitSwipeActionsConfiguration *)trailingActionsForContent:(UIView *)content
{
  if (![self isSwipeContentValid:content] || ![_userDelegate respondsToSelector:@selector(listView:trailingSwipeActionsForItemAtIndex:)]) return nil;
  ShadowListKitSwipeActionsConfiguration *configuration = [_userDelegate listView:self trailingSwipeActionsForItemAtIndex:((ShadowListKitListCell *)content).index];
  return configuration.actions.count ? configuration : nil;
}

- (void)unhighlightSwipeContent:(UIView *)content
{
  ShadowListKitListCell *cell = (id)content;
  if (cell.isHighlighted) [cell setHighlighted:NO animated:NO];
}

- (NSArray<UIAccessibilityCustomAction *> *)swipeAccessibilityActionsForCell:(ShadowListKitListCell *)cell
{
  return [_swipeController accessibilityActionsForContent:cell];
}

#pragma mark - Menu host

- (UIView *)menuRootView { return self; }
- (BOOL)menusEnabled { return [_userDelegate respondsToSelector:@selector(listView:contextMenuForItemAtIndex:)]; }
- (UIView *)menuContentAtPoint:(CGPoint)point { return [self itemCellAtPoint:point]; }
- (BOOL)isMenuContentValid:(UIView *)content { return [self isCurrentActionCell:content]; }
- (BOOL)canShowMenuForContent:(UIView *)content { return ![self hasHeldRow] && ![self isSwipeOpen] && ![self canMoveCell:(id)content]; }
- (UIMenu *)menuForContent:(UIView *)content { return [self menuForCell:(id)content]; }
- (id<NSCopying>)menuIdentifierForContent:(UIView *)content
{
  ShadowListKitListCell *cell = (id)content;
  return [self isCurrentActionCell:cell] ? ShadowListKitString(_driver.getKeyAt((std::size_t)cell.row)) : nil;
}
- (UIView *)menuContentForIdentifier:(id<NSCopying>)identifier
{
  if (![(id)identifier isKindOfClass:NSString.class]) return nil;
  auto found = _mounted.find(ShadowListKitStdString((NSString *)identifier));
  return found != _mounted.end() && [self isCurrentActionCell:found->second] ? found->second : nil;
}
- (void)contextMenuWillShowForContent:(UIView *)content {}

- (UIMenu *)menuForCell:(ShadowListKitListCell *)cell
{
  if (!cell || cell.index == NSNotFound || ![_userDelegate respondsToSelector:@selector(listView:contextMenuForItemAtIndex:)]) return nil;
  return [_userDelegate listView:self contextMenuForItemAtIndex:cell.index];
}

- (BOOL)canMoveCell:(ShadowListKitListCell *)cell
{
  if (!self.reorderEnabled || _editing) return NO;
  return ![_userDelegate respondsToSelector:@selector(listView:canMoveItemAtIndex:)] || [_userDelegate listView:self canMoveItemAtIndex:cell.index];
}

- (BOOL)showMenuForCell:(ShadowListKitListCell *)cell atPoint:(CGPoint)point
{
  return [_menuController showMenuForContent:cell atPoint:point];
}

#pragma mark - Long press

/*
 * The item a hold at a point is a long press on, or nil. A hold lifts a row that can move. On a
 * row that cannot it shows the row's menu, and without a menu the delegate hears a long press.
 * A hold that is none leaves the press to the menu and the lift.
 */
- (ShadowListKitListCell *)longPressCellAtPoint:(CGPoint)point
{
  ShadowListKitListCell *cell = [self itemCellAtPoint:point];
  if (!cell || [self hasHeldRow] || [self isSwipeOpen] || [self canMoveCell:cell] || [self menuForCell:cell]) {
    return nil;
  }
  return cell;
}

- (void)handleLongPress:(UILongPressGestureRecognizer *)press
{
  if (press.state != UIGestureRecognizerStateBegan) {
    return;
  }
  ShadowListKitListCell *cell = [self longPressCellAtPoint:[press locationInView:self]];
  if (cell) {
    [self longPressCell:cell];
  }
}

- (void)longPressCell:(ShadowListKitListCell *)cell
{
  if (cell.index == NSNotFound || ![_userDelegate respondsToSelector:@selector(listView:didLongPressItemAtIndex:)]) {
    return;
  }
  [self unhighlight];
  // The touch selects nothing when it ends.
  _tap.enabled = NO;
  _tap.enabled = YES;
  [_userDelegate listView:self didLongPressItemAtIndex:cell.index];
}

@end

#pragma mark - Testing

@implementation ShadowListKitListView (ActionsTesting)

- (void)shadowListKit_swipeItemAtIndex:(NSInteger)index distance:(CGFloat)distance velocity:(CGFloat)velocity
{
  ShadowListKitListCell *cell = [self cellForItemAtIndex:index];
  if (cell) {
    [self scriptSwipeOfCell:cell distance:distance velocity:velocity];
  }
}

@end
