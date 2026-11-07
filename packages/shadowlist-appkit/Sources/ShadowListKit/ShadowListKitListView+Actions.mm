#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitSwipeActionsView.h"

#import <ShadowListKit/ShadowListKitListView+Testing.h>

#import <QuartzCore/QuartzCore.h>

#include <shadowlist-core/host/ScrollPhysics.hpp>

#include <algorithm>
#include <cmath>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewActionsLink = 0;

/*
 * How long a released row takes to rest, the core's SWIPE_SETTLE_DURATION_MS.
 */
static const NSTimeInterval SHADOWLIST_KIT_SWIPE_DURATION = SWIPE_SETTLE_DURATION_MS / 1000.0;

/*
 * The shortest time between two wheel events a swipe's velocity counts, one display frame.
 * Events that come back to back count a frame apart.
 */
static const NSTimeInterval SHADOWLIST_KIT_SWIPE_FRAME = 1.0 / 120;

/*
 * Fingers that rested this long before they lifted let the row go without velocity.
 */
static const NSTimeInterval SHADOWLIST_KIT_SWIPE_REST = 0.05;

/*
 * The key of the animation that shows a swiped cell moved across the axis.
 */
static NSString *const SHADOWLIST_KIT_SWIPE_KEY = @"shadowListKitSwipe";

static const unsigned short SHADOWLIST_KIT_ESCAPE_KEY_CODE = 53;

/*
 * The outline around the item whose menu is open, the way NSTableView draws its clicked row.
 */
static const CGFloat SHADOWLIST_KIT_MENU_OUTLINE_WIDTH = 2;
static const CGFloat SHADOWLIST_KIT_MENU_OUTLINE_RADIUS = 4;

/*
 * Show a cell moved across the axis by shift. The list places a cell with its frame, a drag
 * shifts it with its layer's transform and a change animation slides it with additive position
 * animations under its own keys. A swipe holds one more additive position animation under
 * SHADOWLIST_KIT_SWIPE_KEY. It adds to all of them and none of them reset it.
 */
static void ShadowListKitShowSwipeShift(CALayer *layer, CGPoint shift)
{
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [layer removeAnimationForKey:SHADOWLIST_KIT_SWIPE_KEY];
  if (shift.x != 0 || shift.y != 0) {
    CABasicAnimation *hold = [CABasicAnimation animationWithKeyPath:@"position"];
    hold.fromValue = [NSValue valueWithPoint:NSMakePoint(shift.x, shift.y)];
    hold.toValue = hold.fromValue;
    hold.additive = YES;
    hold.duration = 1;
    hold.fillMode = kCAFillModeForwards;
    hold.removedOnCompletion = NO;
    [layer addAnimation:hold forKey:SHADOWLIST_KIT_SWIPE_KEY];
  }
  [CATransaction commit];
}

@interface ShadowListKitListView (SwipeSteps)
- (void)swipeLinkTick:(CADisplayLink *)link;
- (void)advanceSwipeSettleBy:(double)seconds;
- (void)scriptSwipeOfCell:(ShadowListKitListCell *)cell distance:(CGFloat)distance velocity:(CGFloat)velocity;
- (void)performSwipeAction:(nullable ShadowListKitSwipeAction *)action;
@end

/*
 * The settle display link's target. It holds the list weakly. A display link retains its
 * target, and the list would otherwise stay alive until the row rests.
 */
@interface ShadowListKitSwipeLinkTarget : NSObject
@property (nonatomic, weak) ShadowListKitListView *list;
@end

@implementation ShadowListKitSwipeLinkTarget

- (void)tick:(CADisplayLink *)link
{
  ShadowListKitListView *list = _list;
  if (!list) {
    [link invalidate];
    return;
  }
  [list swipeLinkTick:link];
}

@end

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * Swipe actions and context menus, the way NSTableView shows row actions. A two finger swipe
 * across the scroll axis on a trackpad moves an item that has actions and shows its buttons.
 * SwipeReveal in the host layer decides how far it follows and where it rests. A mouse wheel
 * never swipes. A right click, a Control-click or the menu key on an item shows the delegate's
 * menu for it, or a menu of its swipe actions. While it is open the item is clickedIndex and its
 * cell the menu target, drawn with an outline.
 */
@implementation ShadowListKitListView (Actions)

- (void)updateSwipesItems
{
  _swipesItems = [_delegate respondsToSelector:@selector(listView:leadingSwipeActionsForItemAtIndex:)] ||
    [_delegate respondsToSelector:@selector(listView:trailingSwipeActionsForItemAtIndex:)];
}

- (BOOL)isSwipeOpen
{
  return _swipeCell != nil;
}

/*
 * A row sliding out after a full swipe counts as out already.
 */
- (BOOL)isSwipedOutCell:(ShadowListKitListCell *)cell
{
  return cell == _swipeCell && _swipe.isSwipedOut(_swipeLink ? _swipeRest.offset : _swipeOffset);
}

- (void)closeSwipeActionsAnimated:(BOOL)animated
{
  [self closeSwipeAnimated:animated];
}

#pragma mark - Swipe gesture

- (ShadowListKitSwipeActionsConfiguration *)leadingActionsForItem:(NSInteger)index
{
  if (index == NSNotFound || ![_delegate respondsToSelector:@selector(listView:leadingSwipeActionsForItemAtIndex:)]) {
    return nil;
  }
  ShadowListKitSwipeActionsConfiguration *configuration = [_delegate listView:self leadingSwipeActionsForItemAtIndex:index];
  return configuration.actions.count > 0 ? configuration : nil;
}

- (ShadowListKitSwipeActionsConfiguration *)trailingActionsForItem:(NSInteger)index
{
  if (index == NSNotFound || ![_delegate respondsToSelector:@selector(listView:trailingSwipeActionsForItemAtIndex:)]) {
    return nil;
  }
  ShadowListKitSwipeActionsConfiguration *configuration = [_delegate listView:self trailingSwipeActionsForItemAtIndex:index];
  return configuration.actions.count > 0 ? configuration : nil;
}

/*
 * Where the pointer was for an event, in the list's coordinates.
 */
- (NSPoint)swipePointOfEvent:(NSEvent *)event
{
  NSWindow *window = event.window;
  NSPoint screen = window ? [window convertPointToScreen:event.locationInWindow] : event.locationInWindow;
  return [self convertPoint:[self.window convertPointFromScreen:screen] fromView:nil];
}

/*
 * The engine passes on a trackpad gesture that starts more across the axis than along it, from
 * the event that decided it to its last momentum event. The list takes the whole gesture when
 * it starts on an item with actions on the side it reveals, or on the open item. The row follows
 * the fingers and settles when they lift, and the momentum after goes nowhere. Other gestures
 * across the axis go to the next responder, like a swipe between pages. A delegate without swipe
 * actions costs one check.
 */
- (BOOL)swipeWithScrollWheel:(NSEvent *)event
{
  if (!_swipesItems && !_swipeCell && !_swipeTracking && !_swipeMomentum) {
    return NO;
  }
  NSEventPhase phase = event.phase;
  NSEventPhase momentum = event.momentumPhase;
  if (momentum != NSEventPhaseNone) {
    BOOL claimed = _swipeMomentum;
    if (momentum & (NSEventPhaseEnded | NSEventPhaseCancelled)) {
      _swipeMomentum = NO;
    }
    return claimed;
  }
  _swipeMomentum = NO;
  if (phase == NSEventPhaseNone) {
    return NO;
  }
  BOOL ends = (phase & (NSEventPhaseEnded | NSEventPhaseCancelled)) != 0;
  if (phase & (NSEventPhaseBegan | NSEventPhaseMayBegin)) {
    _swipeTracking = NO;
    _swipePassing = NO;
  }
  if (_swipePassing) {
    _swipePassing = !ends;
    return NO;
  }
  CGFloat delta = [self cross:CGPointMake(event.scrollingDeltaX, event.scrollingDeltaY)];
  if (!_swipeTracking) {
    if (ends || delta == 0) {
      return NO;
    }
    if (![self beginSwipeAtPoint:[self swipePointOfEvent:event] towardLeading:delta > 0]) {
      _swipePassing = YES;
      return NO;
    }
    _swipeTracking = YES;
    _swipeTranslation = 0;
    _swipeVelocity = 0;
    _swipeTime = event.timestamp - SHADOWLIST_KIT_SWIPE_FRAME;
  }
  if (ends) {
    [self endSwipeAtTime:event.timestamp];
    _swipeMomentum = YES;
    return YES;
  }
  [self moveSwipeBy:delta time:event.timestamp];
  return YES;
}

/*
 * Open the actions of the item under a point to swipe it, or take the open one. A gesture on
 * another item closes the open one first. Nothing swipes while a row is held or pressed.
 */
- (BOOL)beginSwipeAtPoint:(NSPoint)point towardLeading:(BOOL)towardLeading
{
  if ([self hasHeldRow] || _dragPressed) {
    return NO;
  }
  ShadowListKitListCell *cell = [self itemCellAtPoint:point];
  if (!cell) {
    [self closeSwipeAnimated:YES];
    return NO;
  }
  if (cell != _swipeCell) {
    [self closeSwipeAnimated:NO];
    if (!(towardLeading ? [self leadingActionsForItem:cell.index] : [self trailingActionsForItem:cell.index])) {
      return NO;
    }
    [self openActionsForCell:cell];
  }
  if (!_swipeCell) {
    return NO;
  }
  [self stopSwipeSettle];
  [self unhighlight];
  _swipe.begin(_swipe.getSpec(), _swipeOffset);
  return YES;
}

/*
 * The row follows the fingers. Each event's velocity counts half, the events before it the other
 * half.
 */
- (void)moveSwipeBy:(CGFloat)delta time:(NSTimeInterval)time
{
  if (!_swipeCell) {
    return;
  }
  NSTimeInterval elapsed = std::max(time - _swipeTime, SHADOWLIST_KIT_SWIPE_FRAME);
  CGFloat velocity = (CGFloat)(delta / elapsed);
  _swipeVelocity = _swipeTranslation == 0 ? velocity : (_swipeVelocity + velocity) / 2;
  _swipeTime = time;
  _swipeTranslation += delta;
  BOOL wasPast = _swipe.isPastFullSwipe(_swipeOffset);
  [self setSwipeOffset:(CGFloat)_swipe.drag(_swipeTranslation)];
  if (_swipe.isPastFullSwipe(_swipeOffset) != wasPast) {
    [NSHapticFeedbackManager.defaultPerformer performFeedbackPattern:NSHapticFeedbackPatternAlignment
                                                    performanceTime:NSHapticFeedbackPerformanceTimeNow];
  }
}

- (void)endSwipeAtTime:(NSTimeInterval)time
{
  _swipeTracking = NO;
  if (!_swipeCell) {
    return;
  }
  CGFloat velocity = time - _swipeTime > SHADOWLIST_KIT_SWIPE_REST ? 0 : _swipeVelocity;
  [self settleSwipeTo:_swipe.settle(_swipeOffset, velocity, SWIPE_FLING_VELOCITY)];
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
  [self stopSwipeSettle];
  _swipe.begin(_swipe.getSpec(), _swipeOffset);
  [self setSwipeOffset:(CGFloat)_swipe.drag(distance)];
  [self settleSwipeTo:_swipe.settle(_swipeOffset, velocity, SWIPE_FLING_VELOCITY)];
}

/*
 * Put the actions of a row under it, closed.
 */
- (void)openActionsForCell:(ShadowListKitListCell *)cell
{
  if (!cell || cell.index == NSNotFound) {
    return;
  }
  ShadowListKitSwipeActionsConfiguration *leading = [self leadingActionsForItem:cell.index];
  ShadowListKitSwipeActionsConfiguration *trailing = [self trailingActionsForItem:cell.index];
  if (!leading && !trailing) {
    return;
  }
  ShadowListKitSwipeActionsView *view = [[ShadowListKitSwipeActionsView alloc] initWithLeading:leading trailing:trailing horizontal:_horizontal];
  __weak ShadowListKitListView *weakSelf = self;
  view.onAction = ^(ShadowListKitSwipeAction *action) {
    [weakSelf performSwipeAction:action];
  };
  view.hidden = YES;
  ShadowListKitPlace(view, cell.frame);
  [_contentView addSubview:view positioned:NSWindowBelow relativeTo:cell];
  SwipeSpec spec;
  spec.leadingWidth = [view leadingWidth];
  spec.trailingWidth = [view trailingWidth];
  spec.leadingFullSwipe = leading.performsFirstActionWithFullSwipe;
  spec.trailingFullSwipe = trailing.performsFirstActionWithFullSwipe;
  spec.rowSize = _horizontal ? NSHeight(cell.frame) : NSWidth(cell.frame);
  _swipe.begin(spec, 0);
  _swipeCell = cell;
  _swipeActionsView = view;
  _swipeOffset = 0;
  [self startSwipeKeyMonitor];
}

- (void)setSwipeOffset:(CGFloat)offset
{
  _swipeOffset = offset;
  ShadowListKitShowSwipeShift(_swipeCell.layer, _horizontal ? CGPointMake(0, offset) : CGPointMake(offset, 0));
  _swipeActionsView.hidden = offset == 0;
  if (offset != 0) {
    [_swipeActionsView layoutForOffset:offset full:_swipe.isPastFullSwipe(offset)];
  }
}

#pragma mark - Swipe settle

/*
 * The row eases to where it rests on the display link, its buttons following each frame. A full
 * swipe performs the side's first action as the row slides out.
 */
- (void)settleSwipeTo:(SwipeRest)rest
{
  [self stopSwipeSettle];
  _swipeRest = rest;
  _swipeSettleFrom = _swipeOffset;
  _swipeSettleElapsed = 0;
  _swipeFrameTime = 0;
  if (!self.window || _swipeOffset == (CGFloat)rest.offset) {
    [self advanceSwipeSettleBy:SHADOWLIST_KIT_SWIPE_DURATION];
  } else {
    ShadowListKitSwipeLinkTarget *target = [ShadowListKitSwipeLinkTarget new];
    target.list = self;
    _swipeLink = [self displayLinkWithTarget:target selector:@selector(tick:)];
    [_swipeLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
  }
  if (rest.full) {
    ShadowListKitSwipeActionsConfiguration *configuration = rest.side == SwipeSide::Leading ? _swipeActionsView.leading
                                                                                            : _swipeActionsView.trailing;
    [self performSwipeAction:configuration.actions.firstObject];
  }
}

- (void)swipeLinkTick:(CADisplayLink *)link
{
  CFTimeInterval now = link.targetTimestamp;
  double step = _swipeFrameTime > 0 ? now - _swipeFrameTime : link.duration;
  _swipeFrameTime = now;
  [self advanceSwipeSettleBy:step];
}

- (void)advanceSwipeSettleBy:(double)seconds
{
  if (!_swipeCell) {
    [self stopSwipeSettle];
    return;
  }
  _swipeSettleElapsed += seconds;
  double progress = std::min(1.0, _swipeSettleElapsed / SHADOWLIST_KIT_SWIPE_DURATION);
  [self setSwipeOffset:(CGFloat)(_swipeSettleFrom + (_swipeRest.offset - _swipeSettleFrom) * easeOut(progress))];
  if (progress < 1) {
    return;
  }
  [self stopSwipeSettle];
  if (_swipeRest.side == SwipeSide::None && _swipeOffset == 0) {
    [self tearDownSwipe];
  }
}

- (void)stopSwipeSettle
{
  [_swipeLink invalidate];
  _swipeLink = nil;
}

/*
 * Run an action. Its completion closes the row, unless the action deleted it. An action run from
 * a menu or by accessibility has no open row to close.
 */
- (void)performSwipeAction:(ShadowListKitSwipeAction *)action
{
  ShadowListKitListCell *cell = _swipeCell;
  if (!action) {
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
      if (list && cell && list->_swipeCell == cell) {
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
    [self tearDownSwipe];
    return;
  }
  SwipeRest closed;
  [self settleSwipeTo:closed];
}

- (void)tearDownSwipe
{
  [self stopSwipeSettle];
  [self stopSwipeKeyMonitor];
  [_swipeActionsView removeFromSuperview];
  _swipeActionsView = nil;
  if (_swipeCell) {
    ShadowListKitShowSwipeShift(_swipeCell.layer, CGPointZero);
  }
  _swipeCell = nil;
  _swipeOffset = 0;
  // A gesture still moving the row it dropped goes nowhere.
  _swipePassing = _swipePassing || _swipeTracking;
  _swipeTracking = NO;
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
  ShadowListKitPlace(_swipeActionsView, _swipeCell.frame);
}

#pragma mark - Swipe clicks and keys

/*
 * While a row is open, a click anywhere over the content is the list's. The open row's cell
 * covers its buttons, and the cells' own controls would take the click otherwise.
 */
- (NSView *)hitTest:(NSPoint)point
{
  NSView *hit = [super hitTest:point];
  if (_swipeCell && hit && hit != self && [hit isDescendantOf:_contentView]) {
    return self;
  }
  return hit;
}

/*
 * A click while a row is open performs the action under it, or closes the row.
 */
- (BOOL)swipeOwnsClickAtPoint:(NSPoint)point
{
  if (!_swipeCell) {
    return NO;
  }
  NSButton *button = [_swipeActionsView buttonAtPoint:[_swipeActionsView convertPoint:point fromView:self]];
  if (button) {
    [button performClick:nil];
  } else {
    [self closeSwipeAnimated:YES];
  }
  return YES;
}

/*
 * The list may not be the first responder, which leaves Escape to another view. The monitor
 * closes an open row with it and lets every other key through.
 */
- (void)startSwipeKeyMonitor
{
  if (_swipeKeyMonitor) {
    return;
  }
  __weak ShadowListKitListView *weakSelf = self;
  _swipeKeyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent *(NSEvent *event) {
    ShadowListKitListView *list = weakSelf;
    if (![list isSwipeOpen] || event.keyCode != SHADOWLIST_KIT_ESCAPE_KEY_CODE) {
      return event;
    }
    [list closeSwipeAnimated:YES];
    return nil;
  }];
}

- (void)stopSwipeKeyMonitor
{
  if (_swipeKeyMonitor) {
    [NSEvent removeMonitor:_swipeKeyMonitor];
    _swipeKeyMonitor = nil;
  }
}

#pragma mark - Swipe accessibility

/*
 * An item's trailing actions, then its leading ones, run the way a click on their buttons runs
 * them.
 */
- (NSArray<NSAccessibilityCustomAction *> *)accessibilitySwipeActionsForItem:(NSInteger)item
{
  if (!_swipesItems || item == NSNotFound) {
    return @[];
  }
  NSMutableArray<NSAccessibilityCustomAction *> *actions = [NSMutableArray array];
  __weak ShadowListKitListView *weakSelf = self;
  for (ShadowListKitSwipeActionsConfiguration *configuration in [self swipeConfigurationsForItem:item]) {
    for (ShadowListKitSwipeAction *action in configuration.actions) {
      NSString *name = action.title.length > 0 ? action.title : action.image.accessibilityDescription;
      if (name.length == 0) {
        continue;
      }
      [actions addObject:[[NSAccessibilityCustomAction alloc] initWithName:name handler:^BOOL {
        [weakSelf performSwipeAction:action];
        return YES;
      }]];
    }
  }
  return actions;
}

/*
 * The trailing then the leading configuration of an item, without the sides it has none for.
 */
- (NSArray<ShadowListKitSwipeActionsConfiguration *> *)swipeConfigurationsForItem:(NSInteger)item
{
  NSMutableArray<ShadowListKitSwipeActionsConfiguration *> *configurations = [NSMutableArray array];
  ShadowListKitSwipeActionsConfiguration *trailing = [self trailingActionsForItem:item];
  ShadowListKitSwipeActionsConfiguration *leading = [self leadingActionsForItem:item];
  if (trailing) {
    [configurations addObject:trailing];
  }
  if (leading) {
    [configurations addObject:leading];
  }
  return configurations;
}

#pragma mark - Menus

/*
 * The delegate's menu for an item. Without one, an item with swipe actions gets them as a menu.
 */
- (NSMenu *)menuForCell:(ShadowListKitListCell *)cell
{
  if (!cell || cell.index == NSNotFound) {
    return nil;
  }
  if ([_delegate respondsToSelector:@selector(listView:contextMenuForItemAtIndex:)]) {
    NSMenu *menu = [_delegate listView:self contextMenuForItemAtIndex:cell.index];
    if (menu) {
      return menu;
    }
  }
  return _swipesItems ? [self swipeActionsMenuForItem:cell.index] : nil;
}

/*
 * The trailing actions, a separator, then the leading actions. A destructive action's title is
 * red.
 */
- (NSMenu *)swipeActionsMenuForItem:(NSInteger)item
{
  NSArray<ShadowListKitSwipeActionsConfiguration *> *configurations = [self swipeConfigurationsForItem:item];
  if (configurations.count == 0) {
    return nil;
  }
  NSMenu *menu = [NSMenu new];
  for (ShadowListKitSwipeActionsConfiguration *configuration in configurations) {
    if (menu.numberOfItems > 0) {
      [menu addItem:NSMenuItem.separatorItem];
    }
    for (ShadowListKitSwipeAction *action in configuration.actions) {
      NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:action.title ?: @"" action:@selector(performSwipeMenuItem:) keyEquivalent:@""];
      menuItem.target = self;
      menuItem.representedObject = action;
      menuItem.image = action.image;
      if (action.style == ShadowListKitSwipeActionStyleDestructive) {
        menuItem.attributedTitle = [[NSAttributedString alloc] initWithString:menuItem.title attributes:@{
          NSFontAttributeName : [NSFont menuFontOfSize:0],
          NSForegroundColorAttributeName : NSColor.systemRedColor,
        }];
      }
      [menu addItem:menuItem];
    }
  }
  return menu;
}

- (void)performSwipeMenuItem:(NSMenuItem *)menuItem
{
  [self performSwipeAction:menuItem.representedObject];
}

/*
 * AppKit asks for the menu of a right click or a Control-click. The item under it becomes the
 * menu target until the menu closes. An open row closes.
 */
- (NSMenu *)menuForEvent:(NSEvent *)event
{
  [self closeSwipeAnimated:YES];
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
#pragma clang diagnostic pop

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

- (void)shadowListKit_swipeItemAtIndex:(NSInteger)index distance:(CGFloat)distance velocity:(CGFloat)velocity
{
  ShadowListKitListCell *cell = [self cellForItemAtIndex:index];
  if (cell) {
    [self scriptSwipeOfCell:cell distance:distance velocity:velocity];
  }
}

/*
 * The event carries the pointer in global display coordinates, which NSEvent turns into screen
 * coordinates. An event at the global origin tells where that origin is on the screen.
 */
- (void)shadowListKit_swipeWheelDelta:(CGPoint)delta
                                phase:(NSEventPhase)phase
                        momentumPhase:(NSEventPhase)momentumPhase
                              atPoint:(CGPoint)point
{
  CGEventRef wheel = CGEventCreateCopy(ShadowListKitScrollWheelEvent(delta, phase, momentumPhase).CGEvent);
  CGEventSetLocation(wheel, CGPointZero);
  NSEvent *origin = [NSEvent eventWithCGEvent:wheel];
  NSPoint originScreen = origin.window ? [origin.window convertPointToScreen:origin.locationInWindow] : origin.locationInWindow;
  NSPoint screen = [self.window convertPointToScreen:[self convertPoint:point toView:nil]];
  CGEventSetLocation(wheel, CGPointMake(screen.x - originScreen.x, originScreen.y - screen.y));
  NSEvent *event = [NSEvent eventWithCGEvent:wheel];
  CFRelease(wheel);
  [self scrollWheel:event];
  [self layoutSubtreeIfNeeded];
}

/*
 * Settle frames of 1/120 s, the way the display link's frames each move the row.
 */
- (void)shadowListKit_runSwipeFramesFor:(NSTimeInterval)duration
{
  NSTimeInterval frame = 1.0 / 120;
  for (NSTimeInterval run = 0; run < duration && _swipeCell; run += frame) {
    [self advanceSwipeSettleBy:frame];
  }
  [self layoutSubtreeIfNeeded];
}

- (NSInteger)shadowListKit_swipedIndex
{
  return _swipeCell ? _swipeCell.index : NSNotFound;
}

- (CGFloat)shadowListKit_swipeOffset
{
  return _swipeOffset;
}

- (NSView *)shadowListKit_swipeActionsView
{
  return _swipeActionsView;
}

@end
