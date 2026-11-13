#import "Internal/ShadowListKitSwipeController.h"
#import "Internal/ShadowListKitSwipeActionsView.h"
#import "Internal/ShadowListKitListCell+Private.h"

#include <shadowlist-core/host/SwipeReveal.hpp>
#include <cmath>

using namespace azimgd::shadowlist;

@implementation ShadowListKitSwipeController {
  __weak id<ShadowListKitSwipeHost> _host;
  __weak UIView *_content;
  ShadowListKitSwipeActionsView *_actionsView;
  SwipeReveal _swipe;
  CGFloat _offset;
}

- (instancetype)initWithHost:(id<ShadowListKitSwipeHost>)host
{
  if (self = [super init]) {
    _host = host;
    _pan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handlePan:)];
    _pan.delegate = self;
    [host.swipeRootView addGestureRecognizer:_pan];
  }
  return self;
}

- (UIView *)content { return _content; }
- (BOOL)isOpen { return _content != nil; }
- (BOOL)isSwipedOutContent:(UIView *)content { return content == _content && _swipe.isSwipedOut(_offset); }

/* Provider callbacks may reload and reuse a cell before returning its configuration. */
- (BOOL (^)(void))currentContent:(UIView *)content
{
  ShadowListKitListCell *cell = [content isKindOfClass:ShadowListKitListCell.class] ? (id)content : nil;
  NSInteger row = cell.row;
  NSInteger index = cell.index;
  NSUInteger generation = cell.mountGeneration;
  __weak UIView *weakContent = content;
  return ^BOOL {
    UIView *current = weakContent;
    if (!current || ![self->_host isSwipeContentValid:current]) return NO;
    return !cell || (cell.row == row && cell.index == index && cell.mountGeneration == generation);
  };
}

- (UIView *)contentForPan:(UIPanGestureRecognizer *)pan
{
  id<ShadowListKitSwipeHost> host = _host;
  if (!host) return nil;
  CGPoint velocity = [pan velocityInView:host.swipeRootView];
  CGFloat cross = host.horizontal ? velocity.y : velocity.x;
  CGFloat along = host.horizontal ? velocity.x : velocity.y;
  if (std::fabs(cross) <= std::fabs(along)) return nil;
  UIView *content = [host swipeContentAtPoint:[pan locationInView:host.swipeRootView]];
  if (!content) return nil;
  BOOL (^isCurrent)(void) = [self currentContent:content];
  if (!isCurrent()) return nil;
  if (content == _content) return content;
  ShadowListKitSwipeActionsConfiguration *configuration = cross > 0
    ? [host leadingActionsForContent:content] : [host trailingActionsForContent:content];
  return configuration.actions.count && isCurrent() ? content : nil;
}

- (BOOL)takesPan:(UIPanGestureRecognizer *)pan { return [self contentForPan:pan] != nil; }
- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gesture { return gesture == _pan && [self takesPan:_pan]; }

- (void)openContent:(UIView *)content
{
  id<ShadowListKitSwipeHost> host = _host;
  if (!host || !content) return;
  BOOL (^isCurrent)(void) = [self currentContent:content];
  if (!isCurrent()) return;
  ShadowListKitSwipeActionsConfiguration *leading = [host leadingActionsForContent:content];
  if (!isCurrent()) return;
  ShadowListKitSwipeActionsConfiguration *trailing = [host trailingActionsForContent:content];
  if ((!leading.actions.count && !trailing.actions.count) || !isCurrent()) return;
  ShadowListKitSwipeActionsView *view = [[ShadowListKitSwipeActionsView alloc] initWithLeading:leading trailing:trailing horizontal:host.horizontal];
  __weak ShadowListKitSwipeController *weakSelf = self;
  view.onAction = ^(ShadowListKitSwipeAction *action) { [weakSelf performAction:action]; };
  view.bounds = content.bounds;
  view.center = content.center;
  [host.swipeRootView insertSubview:view belowSubview:content];
  SwipeSpec spec;
  spec.leadingWidth = view.leadingWidth;
  spec.trailingWidth = view.trailingWidth;
  spec.leadingFullSwipe = leading.performsFirstActionWithFullSwipe;
  spec.trailingFullSwipe = trailing.performsFirstActionWithFullSwipe;
  spec.rowSize = host.horizontal ? content.bounds.size.height : content.bounds.size.width;
  _swipe.begin(spec, 0);
  _content = content;
  _actionsView = view;
  _offset = 0;
}

- (void)setOffset:(CGFloat)offset
{
  _offset = offset;
  _content.transform = _host.horizontal ? CGAffineTransformMakeTranslation(0, offset) : CGAffineTransformMakeTranslation(offset, 0);
  if (offset != 0) [_actionsView layoutForOffset:offset full:_swipe.isPastFullSwipe(offset)];
}

- (void)settleTo:(SwipeRest)rest
{
  ShadowListKitSwipeActionsView *view = _actionsView;
  [UIView animateWithDuration:SWIPE_SETTLE_DURATION_MS / 1000.0 delay:0 usingSpringWithDamping:1 initialSpringVelocity:0
    options:UIViewAnimationOptionBeginFromCurrentState | UIViewAnimationOptionAllowUserInteraction animations:^{
      [self setOffset:(CGFloat)rest.offset];
      [self->_actionsView layoutIfNeeded];
    } completion:^(BOOL) {
      if (rest.side == SwipeSide::None && view == self->_actionsView && self->_offset == 0) [self reset];
    }];
  if (rest.full) {
    ShadowListKitSwipeActionsConfiguration *configuration = rest.side == SwipeSide::Leading ? view.leading : view.trailing;
    [self performAction:configuration.actions.firstObject];
  }
}

- (void)handlePan:(UIPanGestureRecognizer *)pan
{
  id<ShadowListKitSwipeHost> host = _host;
  switch (pan.state) {
    case UIGestureRecognizerStateBegan: {
      UIView *content = [self contentForPan:pan] ?: [host swipeContentAtPoint:[pan locationInView:host.swipeRootView]];
      if (content != _content) { [self closeAnimated:NO]; [self openContent:content]; }
      if (!_content) return;
      [host unhighlightSwipeContent:_content];
      _swipe.begin(_swipe.getSpec(), _offset);
      break;
    }
    case UIGestureRecognizerStateChanged: {
      if (!_content || ![host isSwipeContentValid:_content]) { [self reset]; return; }
      BOOL wasPast = _swipe.isPastFullSwipe(_offset);
      CGPoint translation = [pan translationInView:host.swipeRootView];
      [self setOffset:(CGFloat)_swipe.drag(host.horizontal ? translation.y : translation.x)];
      if (_swipe.isPastFullSwipe(_offset) != wasPast) [[UIImpactFeedbackGenerator new] impactOccurred];
      break;
    }
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed: {
      if (!_content) return;
      CGPoint velocity = [pan velocityInView:host.swipeRootView];
      [self settleTo:_swipe.settle(_offset, host.horizontal ? velocity.y : velocity.x, SWIPE_FLING_VELOCITY)];
      break;
    }
    default: break;
  }
}

- (void)scriptSwipeOfContent:(UIView *)content distance:(CGFloat)distance velocity:(CGFloat)velocity
{
  if (!content || ![_host isSwipeContentValid:content]) return;
  if (content != _content) { [self closeAnimated:NO]; [self openContent:content]; }
  if (!_content) return;
  _swipe.begin(_swipe.getSpec(), _offset);
  [self setOffset:(CGFloat)_swipe.drag(distance)];
  [self settleTo:_swipe.settle(_offset, velocity, SWIPE_FLING_VELOCITY)];
}

- (void)performAction:(ShadowListKitSwipeAction *)action
{
  UIView *content = _content;
  ShadowListKitSwipeActionsView *view = _actionsView;
  if (!action || !content || ![_host isSwipeContentValid:content]) return;
  if (!action.handler) { [self closeAnimated:YES]; return; }
  __weak ShadowListKitSwipeController *weakSelf = self;
  __weak ShadowListKitSwipeActionsView *weakView = view;
  action.handler(action, ^(BOOL) {
    dispatch_async(dispatch_get_main_queue(), ^{
      ShadowListKitSwipeController *controller = weakSelf;
      if (controller && weakView && controller->_actionsView == weakView) [controller closeAnimated:YES];
    });
  });
}

- (void)closeAnimated:(BOOL)animated
{
  if (!_content) return;
  if (animated) [self settleTo:SwipeRest{}];
  else { [self setOffset:0]; [self reset]; }
}

- (void)reset
{
  [_actionsView removeFromSuperview];
  _actionsView = nil;
  if (_content && !CGAffineTransformIsIdentity(_content.transform) && _offset != 0) _content.transform = CGAffineTransformIdentity;
  _content = nil;
  _offset = 0;
}

- (void)contentWillRecycle:(UIView *)content { if (_content == content) [self reset]; }

- (void)layout
{
  if (!_content) return;
  if (_content.hidden || ![_host isSwipeContentValid:_content]) { [self reset]; return; }
  _actionsView.bounds = _content.bounds;
  _actionsView.center = _content.center;
}

- (BOOL)ownsTapAtPoint:(CGPoint)point
{
  if (!_content) return NO;
  if (![_actionsView hasButtonAtPoint:[_host.swipeRootView convertPoint:point toView:_actionsView]]) [self closeAnimated:YES];
  return YES;
}

- (BOOL)hasButtonAtPoint:(CGPoint)point
{
  return [_actionsView hasButtonAtPoint:[_host.swipeRootView convertPoint:point toView:_actionsView]];
}

- (NSArray<UIAccessibilityCustomAction *> *)accessibilityActionsForContent:(UIView *)content
{
  BOOL (^isCurrent)(void) = [self currentContent:content];
  if (!isCurrent()) return @[];
  NSMutableArray<UIAccessibilityCustomAction *> *actions = [NSMutableArray array];
  __weak ShadowListKitSwipeController *weakSelf = self;
  __weak UIView *weakContent = content;
  for (BOOL leading : {YES, NO}) {
    ShadowListKitSwipeActionsConfiguration *configuration = leading
      ? [_host leadingActionsForContent:content] : [_host trailingActionsForContent:content];
    if (!isCurrent()) return @[];
    for (NSUInteger index = 0; index < configuration.actions.count; ++index) {
      ShadowListKitSwipeAction *action = configuration.actions[index];
      [actions addObject:[[UIAccessibilityCustomAction alloc] initWithName:action.title ?: @"" actionHandler:^BOOL(UIAccessibilityCustomAction *) {
        ShadowListKitSwipeController *controller = weakSelf;
        UIView *current = weakContent;
        if (!controller || !current || ![controller->_host isSwipeContentValid:current]) return NO;
        BOOL (^isCurrent)(void) = [controller currentContent:current];
        ShadowListKitSwipeActionsConfiguration *latest = leading
          ? [controller->_host leadingActionsForContent:current] : [controller->_host trailingActionsForContent:current];
        if (index >= latest.actions.count || !isCurrent()) return NO;
        ShadowListKitSwipeAction *currentAction = latest.actions[index];
        if (controller->_content == current) [controller performAction:currentAction];
        else if (currentAction.handler) currentAction.handler(currentAction, ^(BOOL) {});
        return YES;
      }]];
    }
  }
  return actions;
}

@end
