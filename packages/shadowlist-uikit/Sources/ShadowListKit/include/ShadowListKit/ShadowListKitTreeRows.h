#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * One tree node encoded in source order, or one visible row read back from the shared core.
 * Source rows are depth first, with roots at depth zero and unique stable keys.
 */
@interface ShadowListKitTreeRow : NSObject
@property (nonatomic, copy, readonly) NSString *key;
@property (nonatomic, readonly) NSUInteger depth;
@property (nonatomic, readonly) BOOL hasChildren;
@property (nonatomic, readonly, getter=isExpanded) BOOL expanded;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)initWithKey:(NSString *)key depth:(NSUInteger)depth hasChildren:(BOOL)hasChildren expanded:(BOOL)expanded;
@end

/*
 * Visible tree rows owned by the shared TreeSource. The host encodes its own objects once;
 * the core walks expansion state and splices branches. All calls run on the UI thread.
 */
@interface ShadowListKitTreeRows : NSObject
@property (nonatomic, copy, readonly) NSArray<ShadowListKitTreeRow *> *rows;
@property (nonatomic, copy, readonly) NSArray<NSString *> *expandedKeys;
- (void)resetRows:(NSArray<ShadowListKitTreeRow *> *)rows NS_SWIFT_NAME(reset(_:));
- (void)setExpandedKeys:(NSArray<NSString *> *)keys;
- (void)toggleKey:(NSString *)key;
- (void)expandRow:(NSUInteger)index;
- (void)collapseRow:(NSUInteger)index;
@end

NS_ASSUME_NONNULL_END
