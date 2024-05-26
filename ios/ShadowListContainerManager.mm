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

RCT_EXPORT_VIEW_PROPERTY(onVisibleChange, RCTDirectEventBlock)
RCT_EXPORT_VIEW_PROPERTY(onEndReached, RCTDirectEventBlock)

@end
