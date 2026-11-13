#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Typed copies of the shared AssistantStep variant. A step is text or a group of call indices.
 */
@interface ShadowListUtilsAssistantStep : NSObject
@end

@interface ShadowListUtilsAssistantTextStep : ShadowListUtilsAssistantStep
@property (nonatomic, copy, readonly) NSString *text;
@property (nonatomic, readonly) NSUInteger at;
@end

@interface ShadowListUtilsAssistantCallsStep : ShadowListUtilsAssistantStep
@property (nonatomic, copy, readonly) NSArray<NSNumber *> *callIndices;
@end

FOUNDATION_EXPORT NSArray<ShadowListUtilsAssistantStep *> *ShadowListUtilsStepsOf(NSString *content, NSArray<NSNumber *> *positions);

NS_ASSUME_NONNULL_END
