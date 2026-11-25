#import "Internal/ShadowListKitViewabilityConfig.h"

@implementation ShadowListKitViewabilityConfig
- (instancetype)init
{
  return [self initWithItemVisiblePercentThreshold:nil
                  viewAreaCoveragePercentThreshold:nil
                                   minimumViewTime:0
                                waitForInteraction:NO];
}
- (instancetype)initWithItemVisiblePercentThreshold:(NSNumber *)itemVisiblePercentThreshold
                   viewAreaCoveragePercentThreshold:(NSNumber *)viewAreaCoveragePercentThreshold
                                    minimumViewTime:(double)minimumViewTime
                                 waitForInteraction:(BOOL)waitForInteraction
{
  if (self = [super init]) {
    _itemVisiblePercentThreshold = [itemVisiblePercentThreshold copy];
    _viewAreaCoveragePercentThreshold = [viewAreaCoveragePercentThreshold copy];
    _minimumViewTime = minimumViewTime;
    _waitForInteraction = waitForInteraction;
  }
  return self;
}
- (id)copyWithZone:(NSZone *)zone
{
  return self;
}
@end

@implementation ShadowListKitViewToken
- (instancetype)initWithKey:(NSString *)key index:(NSInteger)index section:(NSInteger)section viewable:(BOOL)viewable
{
  if (self = [super init]) {
    _key = [key copy];
    _index = index;
    _section = section;
    _viewable = viewable;
  }
  return self;
}
@end

@implementation ShadowListKitViewableItemsChangedInfo
- (instancetype)initWithViewableItems:(NSArray<ShadowListKitViewToken *> *)viewableItems
                              changed:(NSArray<ShadowListKitViewToken *> *)changed
{
  if (self = [super init]) {
    _viewableItems = [viewableItems copy];
    _changed = [changed copy];
  }
  return self;
}
@end

@implementation ShadowListKitViewabilityConfigCallbackPair
- (instancetype)initWithViewabilityConfig:(ShadowListKitViewabilityConfig *)viewabilityConfig
                   onViewableItemsChanged:(void (^)(ShadowListKitViewableItemsChangedInfo *))onViewableItemsChanged
{
  if (self = [super init]) {
    _viewabilityConfig = [viewabilityConfig copy];
    _onViewableItemsChanged = [onViewableItemsChanged copy];
  }
  return self;
}
@end

azimgd::shadowlist::ViewabilityConfig ShadowListKitReadViewabilityConfig(ShadowListKitViewabilityConfig *value)
{
  azimgd::shadowlist::ViewabilityConfig config;
  if (value.itemVisiblePercentThreshold) {
    config.itemVisiblePercentThreshold = value.itemVisiblePercentThreshold.doubleValue;
  }
  if (value.viewAreaCoveragePercentThreshold) {
    config.viewAreaCoveragePercentThreshold = value.viewAreaCoveragePercentThreshold.doubleValue;
  }
  config.minimumViewTime = value.minimumViewTime;
  config.waitForInteraction = value.waitForInteraction;
  return config;
}
