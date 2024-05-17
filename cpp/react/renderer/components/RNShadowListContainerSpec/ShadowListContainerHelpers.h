#import <Foundation/Foundation.h>
#import <React/RCTDefines.h>
#import <React/RCTLog.h>

NS_ASSUME_NONNULL_BEGIN

@protocol RCTShadowListContainerViewProtocol <NSObject>

- (void)scrollToIndexNativeCommand:(int)index;
- (void)scrollToOffsetNativeCommand:(int)offset;

@end

RCT_EXTERN inline void RCTShadowListContainerHandleCommand(
  id<RCTShadowListContainerViewProtocol> componentView,
  NSString const *commandName,
  NSArray const *args)
{
  if ([commandName isEqualToString:@"scrollToIndex"]) {
    NSObject *arg0 = args[0];
    [componentView scrollToIndexNativeCommand:[(NSNumber *)arg0 intValue]];
    return;
  }

  if ([commandName isEqualToString:@"scrollToOffset"]) {
    NSObject *arg0 = args[0];
    [componentView scrollToOffsetNativeCommand:[(NSNumber *)arg0 intValue]];
    return;
  }
}

NS_ASSUME_NONNULL_END
