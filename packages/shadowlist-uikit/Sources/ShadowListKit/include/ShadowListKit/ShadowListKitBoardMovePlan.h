#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * The shared board's resolved source and destination. Missing indices are NSNotFound.
 * An empty plan changes nothing. Hosts remove matching keys and insert at destinationIndex.
 */
@interface ShadowListKitBoardMovePlan : NSObject
@property (nonatomic, readonly) NSUInteger sourceColumn;
@property (nonatomic, readonly) NSUInteger sourceIndex;
@property (nonatomic, readonly) NSUInteger destinationColumn;
@property (nonatomic, readonly) NSUInteger destinationIndex;
@property (nonatomic, readonly, getter=isEmpty) BOOL empty;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
+ (instancetype)planWithColumns:(NSArray<NSArray<NSString *> *> *)columns itemKey:(NSString *)itemKey
             destinationColumn:(NSUInteger)destinationColumn destinationKey:(NSString *)destinationKey
              destinationIndex:(NSUInteger)destinationIndex NS_SWIFT_NAME(plan(columns:itemKey:destinationColumn:destinationKey:destinationIndex:));
@end

NS_ASSUME_NONNULL_END
