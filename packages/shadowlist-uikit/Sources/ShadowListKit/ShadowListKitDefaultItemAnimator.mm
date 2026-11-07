#import <ShadowListKit/ShadowListKitListView.h>

#include <shadowlist-core/host/ChangeAnimation.hpp>

/*
 * Default duration of the change animations, in seconds.
 */
static const NSTimeInterval SHADOWLIST_KIT_CHANGE_DURATION = azimgd::shadowlist::CHANGE_DURATION_MS / 1000.0;

@implementation ShadowListKitDefaultItemAnimator

- (instancetype)init
{
  if (self = [super init]) {
    _duration = SHADOWLIST_KIT_CHANGE_DURATION;
  }
  return self;
}

- (void)listView:(ShadowListKitListView *)listView animateInsertOfCell:(ShadowListKitListCell *)cell
{
  cell.transform = CGAffineTransformIdentity;
  cell.alpha = 0;
  [UIView animateWithDuration:_duration animations:^{
    cell.alpha = 1;
  }];
}

- (void)listView:(ShadowListKitListView *)listView animateDeleteOfCell:(ShadowListKitListCell *)cell completion:(void (^)(void))completion
{
  [UIView animateWithDuration:_duration animations:^{
    cell.alpha = 0;
  } completion:^(BOOL) {
    completion();
  }];
}

- (void)listView:(ShadowListKitListView *)listView animateMoveOfCell:(ShadowListKitListCell *)cell fromOffset:(CGPoint)offset
{
  if (offset.x == 0 && offset.y == 0 && CGAffineTransformIsIdentity(cell.transform)) {
    return;
  }
  cell.transform = CGAffineTransformMakeTranslation(offset.x, offset.y);
  [UIView animateWithDuration:_duration delay:0 options:UIViewAnimationOptionBeginFromCurrentState animations:^{
    cell.transform = CGAffineTransformIdentity;
    cell.alpha = 1;
  } completion:nil];
}

@end
