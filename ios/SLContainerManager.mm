#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>
#import "RCTBridge.h"

@interface SLContainerManager : RCTViewManager
@end

@implementation SLContainerManager

RCT_EXPORT_MODULE(SLContainer)

- (UIView *)view
{
  return [[UIView alloc] init];
}

@end
