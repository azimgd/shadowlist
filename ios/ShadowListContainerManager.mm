#import "ShadowListContainerHelpers.h"
#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>

@interface ShadowListContainerManager : RCTViewManager
@end

@implementation ShadowListContainerManager

RCT_EXPORT_MODULE(ShadowListContainer)

- (UIView *)view
{
  return [[UIView alloc] init];
}

@end
