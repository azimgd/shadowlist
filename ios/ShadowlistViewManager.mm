#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>
#import "RCTBridge.h"

@interface ShadowlistViewManager : RCTViewManager
@end

@implementation ShadowlistViewManager

RCT_EXPORT_MODULE(ShadowlistView)

- (UIView *)view
{
  return [[UIView alloc] init];
}

RCT_EXPORT_VIEW_PROPERTY(color, NSString)

@end
