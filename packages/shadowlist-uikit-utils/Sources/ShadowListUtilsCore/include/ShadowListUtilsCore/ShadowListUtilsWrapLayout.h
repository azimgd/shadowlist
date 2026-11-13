#pragma once

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/* Shared wrapping of native measured child sizes. Frames remain leading-to-trailing. */
@interface ShadowListUtilsWrapLayout : NSObject
@property (nonatomic, copy, readonly) NSArray<NSValue *> *frames;
@property (nonatomic, readonly) CGFloat height;
- (instancetype)initWithWidth:(CGFloat)width sizes:(NSArray<NSValue *> *)sizes gap:(CGFloat)gap;
@end

NS_ASSUME_NONNULL_END
