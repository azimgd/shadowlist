#import <Foundation/Foundation.h>
#import <React/RCTDefines.h>
#import <React/RCTLog.h>

NS_ASSUME_NONNULL_BEGIN

@protocol RCTShadowListContainerViewProtocol <NSObject>

- (void)scrollToIndex:(int)index;
- (void)scrollToOffset:(int)offset;

@end

RCT_EXTERN inline void RCTShadowListContainerHandleCommand(
  id<RCTShadowListContainerViewProtocol> componentView,
  NSString const *commandName,
  NSArray const *args)
{
  if ([commandName isEqualToString:@"scrollToIndex"]) {
    NSObject *arg0 = args[0];
    [componentView scrollToIndex:[(NSNumber *)arg0 intValue]];
    return;
  }

  if ([commandName isEqualToString:@"scrollToOffset"]) {
    NSObject *arg0 = args[0];
    [componentView scrollToOffset:[(NSNumber *)arg0 intValue]];
    return;
  }
}

NS_ASSUME_NONNULL_END
