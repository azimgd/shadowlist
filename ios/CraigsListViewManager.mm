#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>
#import "RCTBridge.h"

@interface CraigsListViewManager : RCTViewManager
@end

@implementation CraigsListViewManager

RCT_EXPORT_MODULE(CraigsListView)

- (UIView *)view
{
  return [[UIView alloc] init];
}

RCT_EXPORT_VIEW_PROPERTY(color, NSString)

@end
