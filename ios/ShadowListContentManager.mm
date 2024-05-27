#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>

@interface ShadowListContentManager : RCTViewManager
@end

@implementation ShadowListContentManager

RCT_EXPORT_MODULE(ShadowListContent)

- (UIView *)view
{
  return [[UIView alloc] init];
}

@end
