#import "Internal/ShadowListKitListView+Private.h"

#import <ShadowListKit/ShadowListKitListView+Testing.h>

#import <QuartzCore/QuartzCore.h>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewActionsLink = 0;

/*
 * The outline around the item whose menu is open, the way NSTableView draws its clicked row.
 */
static const CGFloat SHADOWLIST_KIT_MENU_OUTLINE_WIDTH = 2;
static const CGFloat SHADOWLIST_KIT_MENU_OUTLINE_RADIUS = 4;

/*
 * Context menus. A right click, a Control-click or the menu key on an item shows the delegate's
 * menu for it. While it is open the item is clickedIndex and its cell the menu target, drawn
 * with an outline.
 */
@implementation ShadowListKitListView (Actions)

#pragma mark - Menus

- (NSMenu *)menuForCell:(ShadowListKitListCell *)cell
{
  if (!cell || cell.index == NSNotFound || ![_delegate respondsToSelector:@selector(listView:contextMenuForItemAtIndex:)]) {
    return nil;
  }
  return [_delegate listView:self contextMenuForItemAtIndex:cell.index];
}

/*
 * AppKit asks for the menu of a right click or a Control-click. The item under it becomes the
 * menu target until the menu closes.
 */
- (NSMenu *)menuForEvent:(NSEvent *)event
{
  ShadowListKitListCell *cell = [self itemCellAtPoint:[self convertPoint:event.locationInWindow fromView:nil]];
  // A data change since the last layout pass may have dropped the cell's row.
  NSMenu *menu = cell && (std::size_t)cell.row < _driver.getKeyCount() ? [self menuForCell:cell] : nil;
  if (!menu) {
    return [super menuForEvent:event];
  }
  [self beginMenu:menu cell:cell];
  return menu;
}

/*
 * The menu key opens the menu of the keyboard's item below it.
 */
- (void)showMenuForRow:(std::size_t)row
{
  [self revealRow:row];
  ShadowListKitListCell *cell = [self mountedCellAtIndex:row];
  NSMenu *menu = cell.hidden ? nil : [self menuForCell:cell];
  if (!menu) {
    return;
  }
  [self beginMenu:menu cell:cell];
  NSRect frame = [self convertRect:cell.frame fromView:cell.superview];
  [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(NSMinX(frame), NSMaxY(frame)) inView:self];
}

- (void)beginMenu:(NSMenu *)menu cell:(ShadowListKitListCell *)cell
{
  [self endMenuTarget];
  _clickedKey = _driver.getKeyAt((std::size_t)cell.row);
  _hasClickedKey = YES;
  _menuCell = cell;
  [cell setMenuTarget:YES animated:NO];
  [self placeMenuOutlineOnCell:cell];
  [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(menuDidEndTracking:)
                                             name:NSMenuDidEndTrackingNotification object:menu];
}

- (void)menuDidEndTracking:(NSNotification *)notification
{
  [NSNotificationCenter.defaultCenter removeObserver:self name:NSMenuDidEndTrackingNotification object:notification.object];
  [self endMenuTarget];
}

- (void)endMenuTarget
{
  ShadowListKitListCell *cell = _menuCell;
  _menuCell = nil;
  _hasClickedKey = NO;
  _clickedKey.clear();
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [_menuOutline removeFromSuperlayer];
  [CATransaction commit];
  if (cell.isMenuTarget) {
    [cell setMenuTarget:NO animated:NO];
  }
}

/*
 * The outline is a layer over the cell's content, which moves with the cell as the list scrolls.
 */
- (void)placeMenuOutlineOnCell:(ShadowListKitListCell *)cell
{
  if (!cell.layer) {
    return;
  }
  if (!_menuOutline) {
    _menuOutline = [CALayer layer];
    _menuOutline.zPosition = 1001;
    _menuOutline.borderWidth = SHADOWLIST_KIT_MENU_OUTLINE_WIDTH;
    _menuOutline.cornerRadius = SHADOWLIST_KIT_MENU_OUTLINE_RADIUS;
  }
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [self.effectiveAppearance performAsCurrentDrawingAppearance:^{
    self->_menuOutline.borderColor = NSColor.controlAccentColor.CGColor;
  }];
  _menuOutline.frame = CGRectInset(cell.layer.bounds, 1, 1);
  [cell.layer addSublayer:_menuOutline];
  [CATransaction commit];
}

@end

#pragma mark - Testing

@implementation ShadowListKitListView (ActionsTesting)

- (NSMenu *)shadowListKit_menuForItemAtIndex:(NSInteger)index modifierFlags:(NSEventModifierFlags)flags
{
  ShadowListKitListCell *cell = [self cellForItemAtIndex:index];
  NSRect frame = cell ? [self convertRect:cell.frame fromView:cell.superview] : NSZeroRect;
  NSPoint point = [self convertPoint:NSMakePoint(NSMidX(frame), NSMidY(frame)) toView:nil];
  NSEventType type = (flags & NSEventModifierFlagControl) ? NSEventTypeLeftMouseDown : NSEventTypeRightMouseDown;
  NSEvent *event = [NSEvent mouseEventWithType:type location:point modifierFlags:flags
                                     timestamp:NSProcessInfo.processInfo.systemUptime
                                  windowNumber:self.window.windowNumber context:nil eventNumber:0 clickCount:1 pressure:1];
  return [cell menuForEvent:event];
}

- (CALayer *)shadowListKit_menuOutline
{
  return _menuOutline.superlayer ? _menuOutline : nil;
}

@end
