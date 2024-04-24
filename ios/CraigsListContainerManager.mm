#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>
#import "RCTBridge.h"

@interface CraigsListContainerManager : RCTViewManager
@end

@implementation CraigsListContainerManager

RCT_EXPORT_MODULE(CraigsListContainer)

- (UIView *)view
{
  return [[UIView alloc] init];
}

RCT_EXPORT_VIEW_PROPERTY(color, NSString)

@end
