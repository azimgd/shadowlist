#import <ShadowListKit/ShadowListKitListView.h>

#import <QuartzCore/QuartzCore.h>

#include <shadowlist-core/host/ChangeAnimation.hpp>

/*
 * Default duration of the change animations, in seconds.
 */
static const NSTimeInterval SHADOWLIST_KIT_CHANGE_DURATION = azimgd::shadowlist::CHANGE_DURATION_MS / 1000.0;

/*
 * The cell's layers animate with explicit animations only. The model values stay where the list
 * placed them.
 */
@implementation ShadowListKitDefaultItemAnimator

- (instancetype)init
{
  if (self = [super init]) {
    _duration = SHADOWLIST_KIT_CHANGE_DURATION;
  }
  return self;
}

- (CABasicAnimation *)animationOf:(NSString *)keyPath from:(id)from to:(id)to
{
  CABasicAnimation *animation = [CABasicAnimation animationWithKeyPath:keyPath];
  animation.fromValue = from;
  animation.toValue = to;
  animation.duration = _duration;
  animation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
  return animation;
}

- (void)listView:(ShadowListKitListView *)listView animateInsertOfCell:(ShadowListKitListCell *)cell
{
  [cell.layer addAnimation:[self animationOf:@"opacity" from:@0 to:@1] forKey:@"shadowListKitInsert"];
}

- (void)listView:(ShadowListKitListView *)listView animateDeleteOfCell:(ShadowListKitListCell *)cell completion:(void (^)(void))completion
{
  CABasicAnimation *fade = [self animationOf:@"opacity" from:@1 to:@0];
  fade.fillMode = kCAFillModeForwards;
  fade.removedOnCompletion = NO;
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    [cell.layer removeAnimationForKey:@"shadowListKitDelete"];
    completion();
  }];
  [cell.layer addAnimation:fade forKey:@"shadowListKitDelete"];
  [CATransaction commit];
}

- (void)listView:(ShadowListKitListView *)listView animateMoveOfCell:(ShadowListKitListCell *)cell fromOffset:(CGPoint)offset
{
  if (offset.x == 0 && offset.y == 0) {
    return;
  }
  NSValue *from = [NSValue valueWithCATransform3D:CATransform3DMakeTranslation(offset.x, offset.y, 0)];
  NSValue *to = [NSValue valueWithCATransform3D:CATransform3DIdentity];
  CABasicAnimation *slide = [self animationOf:@"transform" from:from to:to];
  slide.additive = YES;
  [cell.layer addAnimation:slide forKey:@"shadowListKitMove"];
}

@end
