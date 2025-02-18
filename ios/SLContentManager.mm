#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>
#import "RCTBridge.h"

@interface SLContentManager : RCTViewManager
@end

@implementation SLContentManager

RCT_EXPORT_MODULE(SLContent)

- (UIView *)view
{
  return [[UIView alloc] init];
}

@end
