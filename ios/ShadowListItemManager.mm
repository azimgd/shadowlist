#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>

@interface ShadowListItemManager : RCTViewManager
@end

@implementation ShadowListItemManager

RCT_EXPORT_MODULE(ShadowListItem)

- (UIView *)view
{
  return [[UIView alloc] init];
}

@end
