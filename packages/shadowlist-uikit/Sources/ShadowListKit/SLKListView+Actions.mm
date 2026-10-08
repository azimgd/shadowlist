#import "Internal/SLKListView+Private.h"

#include <cmath>
#include <numeric>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * How long a released row takes to rest, the core's SWIPE_SETTLE_DURATION_MS.
 */
static const NSTimeInterval SLK_SWIPE_DURATION = SWIPE_SETTLE_DURATION_MS / 1000.0;

#pragma mark - Actions view

/*
 * The buttons behind a swiped row, for both sides. It sits under the row's cell with the
 * cell's resting frame. The side being revealed shows its buttons stretched over the gap the
 * row leaves. Past the full swipe point the first button fills all of it.
 */
@interface SLKSwipeActionsView : UIView
@property (nonatomic, strong, nullable) SLKSwipeActionsConfiguration *leading;
@property (nonatomic, strong, nullable) SLKSwipeActionsConfiguration *trailing;
@property (nonatomic) BOOL horizontal;
@property (nonatomic, copy, nullable) void (^onAction)(SLKSwipeAction *action);
@end

@implementation SLKSwipeActionsView {
  NSArray<UIButton *> *_leadingButtons;
  NSArray<UIButton *> *_trailingButtons;
  std::vector<double> _leadingWidths;
  std::vector<double> _trailingWidths;
  std::vector<SwipeSpan> _spans;
}

- (instancetype)initWithLeading:(SLKSwipeActionsConfiguration *)leading
                       trailing:(SLKSwipeActionsConfiguration *)trailing
                     horizontal:(BOOL)horizontal
{
  if (self = [super initWithFrame:CGRectZero]) {
    _leading = leading;
    _trailing = trailing;
    _horizontal = horizontal;
    self.clipsToBounds = YES;
    _leadingButtons = [self buttonsFor:leading widths:_leadingWidths];
    _trailingButtons = [self buttonsFor:trailing widths:_trailingWidths];
  }
  return self;
}

- (NSArray<UIButton *> *)buttonsFor:(SLKSwipeActionsConfiguration *)configuration widths:(std::vector<double>&)widths
{
  NSMutableArray<UIButton *> *buttons = [NSMutableArray array];
  for (SLKSwipeAction *action in configuration.actions) {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.backgroundColor = action.backgroundColor;
    button.tintColor = UIColor.whiteColor;
    button.titleLabel.font = [UIFont systemFontOfSize:15 weight:UIFontWeightMedium];
    button.clipsToBounds = YES;
    [button setTitle:action.title forState:UIControlStateNormal];
    [button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    [button setImage:action.image forState:UIControlStateNormal];
    button.accessibilityLabel = action.title;
    __weak SLKSwipeActionsView *weakSelf = self;
    [button addAction:[UIAction actionWithHandler:^(UIAction *) {
      SLKSwipeActionsView *view = weakSelf;
      if (view && view->_onAction) {
        view->_onAction(action);
      }
    }] forControlEvents:UIControlEventTouchUpInside];
    CGSize fits = [button sizeThatFits:CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX)];
    widths.push_back(swipeButtonSize(_horizontal ? fits.height : fits.width, 1.0));
    [self addSubview:button];
    [buttons addObject:button];
  }
  return buttons;
}

- (CGFloat)leadingWidth
{
  return (CGFloat)std::accumulate(_leadingWidths.begin(), _leadingWidths.end(), 0.0);
}

- (CGFloat)trailingWidth
{
  return (CGFloat)std::accumulate(_trailingWidths.begin(), _trailingWidths.end(), 0.0);
}

/*
 * Place the buttons for a row moved offset across the axis. full lets the first button fill
 * the gap.
 */
- (void)layoutForOffset:(CGFloat)offset full:(BOOL)full
{
  BOOL leading = offset > 0;
  NSArray<UIButton *> *shown = leading ? _leadingButtons : _trailingButtons;
  NSArray<UIButton *> *other = leading ? _trailingButtons : _leadingButtons;
  for (UIButton *button in other) {
    button.hidden = YES;
  }
  CGSize size = self.bounds.size;
  CGFloat crossSize = _horizontal ? size.height : size.width;
  CGFloat alongSize = _horizontal ? size.width : size.height;
  swipeButtonSpans(leading ? _leadingWidths : _trailingWidths, offset, full, crossSize, _spans);
  for (NSUInteger at = 0; at < shown.count && at < _spans.size(); ++at) {
    UIButton *button = shown[at];
    CGFloat start = (CGFloat)_spans[at].start;
    CGFloat width = (CGFloat)_spans[at].size;
    button.hidden = width <= 0;
    button.frame = _horizontal ? CGRectMake(0, start, alongSize, width) : CGRectMake(start, 0, width, alongSize);
  }
  self.backgroundColor = shown.firstObject.backgroundColor;
  // Only the gap shows the buttons' color.
  SwipeSpan gap = swipeRevealedSpan(offset, crossSize);
  CGRect visible = _horizontal ? CGRectMake(0, (CGFloat)gap.start, alongSize, (CGFloat)gap.size)
                               : CGRectMake((CGFloat)gap.start, 0, (CGFloat)gap.size, alongSize);
  self.layer.mask = nil;
  CALayer *mask = [CALayer layer];
  mask.backgroundColor = UIColor.blackColor.CGColor;
  mask.frame = visible;
  self.layer.mask = mask;
}

- (BOOL)hasButtonAtPoint:(CGPoint)point
{
  for (UIButton *button in [_leadingButtons arrayByAddingObjectsFromArray:_trailingButtons]) {
    if (!button.hidden && CGRectContainsPoint(button.frame, point)) {
      return YES;
    }
  }
  return NO;
}

@end

#pragma mark - Swipe and menus

/*
 * Swipe actions and context menus. A pan across the scroll axis on a row with actions moves
 * the row and shows its buttons. SwipeReveal in the host layer decides how far it follows and
 * where it rests. Touching and holding a row shows its menu through UIContextMenuInteraction,
 * unless the row can be reordered. The hold then lifts the row, and the menu shows when it is let
 * go in place, through UIEditMenuInteraction.
 */
@implementation SLKListView (Actions)

- (void)installActionGestures
{
  if (!_swipePan) {
    _swipePan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handleSwipePan:)];
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

- (BOOL)isSwipedOutCell:(SLKListCell *)cell
{
  return cell == _swipeCell && _swipe.isSwipedOut(_swipeOffset);
}

#pragma mark - Swipe gesture

- (SLKSwipeActionsConfiguration *)leadingActionsForCell:(SLKListCell *)cell
{
  if (![_userDelegate respondsToSelector:@selector(listView:leadingSwipeActionsForItemAtIndex:)]) {
    return nil;
  }
  SLKSwipeActionsConfiguration *configuration = [_userDelegate listView:self leadingSwipeActionsForItemAtIndex:cell.index];
  return configuration.actions.count > 0 ? configuration : nil;
}

- (SLKSwipeActionsConfiguration *)trailingActionsForCell:(SLKListCell *)cell
{
  if (![_userDelegate respondsToSelector:@selector(listView:trailingSwipeActionsForItemAtIndex:)]) {
    return nil;
  }
  SLKSwipeActionsConfiguration *configuration = [_userDelegate listView:self trailingSwipeActionsForItemAtIndex:cell.index];
  return configuration.actions.count > 0 ? configuration : nil;
}

/*
 * The row a pan across the axis would swipe, or nil. A pan along the axis scrolls instead.
 */
- (SLKListCell *)swipeCellForPan:(UIPanGestureRecognizer *)pan
{
  if (_editing || [self hasHeldRow]) {
    return nil;
  }
  CGPoint velocity = [pan velocityInView:self];
  CGFloat cross = [self cross:velocity];
  if (std::fabs(cross) <= std::fabs([self along:velocity])) {
    return nil;
  }
  SLKListCell *cell = [self itemCellAtPoint:[pan locationInView:self]];
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
      SLKListCell *cell = [self swipeCellForPan:pan] ?: [self itemCellAtPoint:[pan locationInView:self]];
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

- (void)scriptSwipeOfCell:(SLKListCell *)cell distance:(CGFloat)distance velocity:(CGFloat)velocity
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
- (void)openActionsForCell:(SLKListCell *)cell
{
  if (!cell) {
    return;
  }
  SLKSwipeActionsConfiguration *leading = [self leadingActionsForCell:cell];
  SLKSwipeActionsConfiguration *trailing = [self trailingActionsForCell:cell];
  if (!leading && !trailing) {
    return;
  }
  SLKSwipeActionsView *view = [[SLKSwipeActionsView alloc] initWithLeading:leading trailing:trailing horizontal:_horizontal];
  __weak SLKListView *weakSelf = self;
  view.onAction = ^(SLKSwipeAction *action) {
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
  SLKListCell *cell = _swipeCell;
  [UIView animateWithDuration:SLK_SWIPE_DURATION delay:0 usingSpringWithDamping:1 initialSpringVelocity:0
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
    SLKSwipeActionsConfiguration *configuration = rest.side == SwipeSide::Leading ? _swipeActionsView.leading
                                                                                    : _swipeActionsView.trailing;
    [self performSwipeAction:configuration.actions.firstObject];
  }
}

/*
 * Run an action. Its completion closes the row, unless the action removed it.
 */
- (void)performSwipeAction:(SLKSwipeAction *)action
{
  SLKListCell *cell = _swipeCell;
  if (!action || !cell) {
    return;
  }
  __weak SLKListView *weakSelf = self;
  action.handler(action, ^(BOOL) {
    dispatch_async(dispatch_get_main_queue(), ^{
      SLKListView *list = weakSelf;
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

- (void)swipeCellWillRecycle:(SLKListCell *)cell
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

- (UIMenu *)menuForCell:(SLKListCell *)cell
{
  if (!cell || cell.index == NSNotFound || ![_userDelegate respondsToSelector:@selector(listView:contextMenuForItemAtIndex:)]) {
    return nil;
  }
  return [_userDelegate listView:self contextMenuForItemAtIndex:cell.index];
}

- (BOOL)canMoveCell:(SLKListCell *)cell
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
  SLKListCell *cell = [self itemCellAtPoint:location];
  // A row that can be reordered lifts on the hold. Its menu waits for the release.
  if (!cell || [self hasHeldRow] || [self isSwipeOpen] || [self canMoveCell:cell]) {
    return nil;
  }
  UIMenu *menu = [self menuForCell:cell];
  if (!menu) {
    return nil;
  }
  NSString *key = @(_driver.getKeyAt((std::size_t)cell.row).c_str());
  return [UIContextMenuConfiguration configurationWithIdentifier:key previewProvider:nil
                                                  actionProvider:^UIMenu *(NSArray<UIMenuElement *> *) {
                                                    return menu;
                                                  }];
}

- (UITargetedPreview *)previewForConfiguration:(UIContextMenuConfiguration *)configuration
{
  NSString *key = (NSString *)configuration.identifier;
  auto mounted = _mounted.find(key.UTF8String ?: "");
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

- (BOOL)showMenuForCell:(SLKListCell *)cell atPoint:(CGPoint)point
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
