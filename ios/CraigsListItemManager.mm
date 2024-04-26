#import <React/RCTViewManager.h>
#import <React/RCTUIManager.h>

@interface CraigsListItemManager : RCTViewManager
@end

@implementation CraigsListItemManager

RCT_EXPORT_MODULE(CraigsListItem)

- (UIView *)view
{
  return [[UIView alloc] init];
}

@end
