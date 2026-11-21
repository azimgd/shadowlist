#pragma once
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Percentages are 0..100; minimumViewTime is milliseconds. Instances are immutable snapshots.
 */
@interface ShadowListKitViewabilityConfig : NSObject <NSCopying>
- (instancetype)init;
- (instancetype)initWithItemVisiblePercentThreshold:(nullable NSNumber *)itemVisiblePercentThreshold
                  viewAreaCoveragePercentThreshold:(nullable NSNumber *)viewAreaCoveragePercentThreshold
                                   minimumViewTime:(double)minimumViewTime
                                waitForInteraction:(BOOL)waitForInteraction NS_DESIGNATED_INITIALIZER;
@property (nonatomic, copy, readonly, nullable) NSNumber *itemVisiblePercentThreshold;
@property (nonatomic, copy, readonly, nullable) NSNumber *viewAreaCoveragePercentThreshold;
@property (nonatomic, readonly) double minimumViewTime;
@property (nonatomic, readonly) BOOL waitForInteraction;
@end

/*
 * Public item identity. section is NSNotFound for an unsectioned list.
 */
@interface ShadowListKitViewToken : NSObject
- (instancetype)initWithKey:(NSString *)key index:(NSInteger)index section:(NSInteger)section viewable:(BOOL)viewable;
@property (nonatomic, copy, readonly) NSString *key;
@property (nonatomic, readonly) NSInteger index;
@property (nonatomic, readonly) NSInteger section;
@property (nonatomic, readonly, getter=isViewable) BOOL viewable;
@end

@interface ShadowListKitViewableItemsChangedInfo : NSObject
- (instancetype)initWithViewableItems:(NSArray<ShadowListKitViewToken *> *)viewableItems
                              changed:(NSArray<ShadowListKitViewToken *> *)changed;
@property (nonatomic, copy, readonly) NSArray<ShadowListKitViewToken *> *viewableItems;
@property (nonatomic, copy, readonly) NSArray<ShadowListKitViewToken *> *changed;
@end

@interface ShadowListKitViewabilityConfigCallbackPair : NSObject
- (instancetype)initWithViewabilityConfig:(ShadowListKitViewabilityConfig *)viewabilityConfig
                   onViewableItemsChanged:(void (^ _Nullable)(ShadowListKitViewableItemsChangedInfo *info))onViewableItemsChanged;
@property (nonatomic, copy, readonly) ShadowListKitViewabilityConfig *viewabilityConfig;
@property (nonatomic, copy, readonly, nullable) void (^onViewableItemsChanged)(ShadowListKitViewableItemsChangedInfo *info);
@end

NS_ASSUME_NONNULL_END
