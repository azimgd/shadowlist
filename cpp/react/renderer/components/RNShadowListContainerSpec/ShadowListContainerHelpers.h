#import <Foundation/Foundation.h>
#import <React/RCTDefines.h>
#import <React/RCTLog.h>

NS_ASSUME_NONNULL_BEGIN

@protocol RCTShadowListContainerViewProtocol <NSObject>

- (void)scrollToIndexNativeCommand:(int)index animated:(BOOL)animated;
- (void)scrollToOffsetNativeCommand:(int)offset animated:(BOOL)animated;

@end

RCT_EXTERN inline void RCTShadowListContainerHandleCommand(
  id<RCTShadowListContainerViewProtocol> componentView,
  NSString const *commandName,
  NSArray const *args)
{
  if ([commandName isEqualToString:@"scrollToIndex"]) {
    NSObject *arg0 = args[0];
    NSObject *arg1 = args[1];
    [componentView scrollToIndexNativeCommand:[(NSNumber *)arg0 intValue] animated:[(NSNumber *)arg1 boolValue]];
    return;
  }

  if ([commandName isEqualToString:@"scrollToOffset"]) {
    NSObject *arg0 = args[0];
    NSObject *arg1 = args[1];
    [componentView scrollToOffsetNativeCommand:[(NSNumber *)arg0 intValue] animated:[(NSNumber *)arg1 boolValue]];
    return;
  }
}

NS_ASSUME_NONNULL_END
