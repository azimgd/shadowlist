#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>
#import "RCTBridge.h"

@interface SLElementManager : RCTViewManager
@end

@implementation SLElementManager

RCT_EXPORT_MODULE(SLElement)

- (UIView *)view
{
  return [[UIView alloc] init];
}

@end
