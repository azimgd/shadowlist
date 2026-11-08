#pragma once

#import <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

NS_ASSUME_NONNULL_BEGIN

/*
 * Text typeset ahead of time. Creating one is thread safe and does all the line breaking. A
 * row's layout can be computed off the main thread, and the main thread only draws the lines.
 * Every line is lineHeight tall, like a label with a fixed minimum and maximum line height.
 * size is the width of the widest line, rounded up, and lineCount lines of lineHeight.
 * drawAtPoint:inContext:color: draws with the top left corner at point, in a context with
 * flipped coordinates like UIKit's or a flipped view's.
 */
NS_SWIFT_SENDABLE
@interface ShadowListKitTextLayout : NSObject

#if TARGET_OS_OSX
+ (instancetype)layoutWithString:(NSString *)string
                            font:(NSFont *)font
                      lineHeight:(CGFloat)lineHeight
                            kern:(CGFloat)kern
                           width:(CGFloat)width
                    maximumLines:(NSUInteger)maximumLines
                       alignment:(NSTextAlignment)alignment;
#else
+ (instancetype)layoutWithString:(NSString *)string
                            font:(UIFont *)font
                      lineHeight:(CGFloat)lineHeight
                            kern:(CGFloat)kern
                           width:(CGFloat)width
                    maximumLines:(NSUInteger)maximumLines
                       alignment:(NSTextAlignment)alignment;
#endif

@property (nonatomic, readonly) CGSize size;
@property (nonatomic, readonly) NSUInteger lineCount;

- (void)drawAtPoint:(CGPoint)point inContext:(CGContextRef)context color:(CGColorRef)color;

@end

/*
 * Shows a ShadowListKitTextLayout. It draws the lines it is given and never measures text itself.
 * On the Mac the lines are drawn into a bitmap at the window's backing scale and shown as the
 * layer's contents. With displaysAsynchronously the bitmap is drawn on a background queue and the
 * view shows it when ready, leaving the main thread only the hand over of the bitmap.
 */
#if TARGET_OS_OSX
@interface ShadowListKitTextView : NSView
#else
@interface ShadowListKitTextView : UIView
#endif

@property (nonatomic, strong, nullable) ShadowListKitTextLayout *textLayout;
#if TARGET_OS_OSX
@property (nonatomic, strong) NSColor *textColor;
#else
@property (nonatomic, strong) UIColor *textColor;
#endif
@property (nonatomic) BOOL displaysAsynchronously;

@end

NS_ASSUME_NONNULL_END
