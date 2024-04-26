#import <Foundation/Foundation.h>
#import <React/RCTDefines.h>
#import <React/RCTLog.h>

NS_ASSUME_NONNULL_BEGIN

@protocol RCTCraigsListContainerViewProtocol <NSObject>

- (void)scrollToIndex:(int)index;

@end

RCT_EXTERN inline void RCTCraigsListContainerHandleCommand(
  id<RCTCraigsListContainerViewProtocol> componentView,
  NSString const *commandName,
  NSArray const *args)
{
  if ([commandName isEqualToString:@"scrollToIndex"]) {
    NSObject *arg0 = args[0];
    [componentView scrollToIndex:[(NSNumber *)arg0 intValue]];
    return;
  }
}

NS_ASSUME_NONNULL_END
