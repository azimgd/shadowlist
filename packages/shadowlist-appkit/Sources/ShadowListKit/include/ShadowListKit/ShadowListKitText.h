#pragma once

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Text typeset ahead of time. Creating one is thread safe and does all the line breaking. A
 * row's layout can be computed off the main thread, and the main thread only draws the lines.
 * Every line is lineHeight tall, like a label with a fixed minimum and maximum line height.
 * size is the width of the widest line, rounded up, and lineCount lines of lineHeight.
 * drawAtPoint:inContext:color: draws with the top left corner at point, in a context with
 * flipped coordinates like a flipped view's.
 */
NS_SWIFT_SENDABLE
@interface ShadowListKitTextLayout : NSObject

+ (instancetype)layoutWithString:(NSString *)string
                            font:(NSFont *)font
                      lineHeight:(CGFloat)lineHeight
                            kern:(CGFloat)kern
                           width:(CGFloat)width
                    maximumLines:(NSUInteger)maximumLines
                       alignment:(NSTextAlignment)alignment;

@property (nonatomic, readonly) CGSize size;
@property (nonatomic, readonly) NSUInteger lineCount;

- (void)drawAtPoint:(CGPoint)point inContext:(CGContextRef)context color:(CGColorRef)color;

@end

/*
 * Shows a ShadowListKitTextLayout. It draws the lines it is given and never measures text itself.
 * The lines are drawn into a bitmap at the window's backing scale and shown as the layer's
 * contents. With displaysAsynchronously the bitmap is drawn on a background queue and the view
 * shows it when ready, leaving the main thread only the hand over of the bitmap.
 */
@interface ShadowListKitTextView : NSView

@property (nonatomic, strong, nullable) ShadowListKitTextLayout *textLayout;
@property (nonatomic, strong) NSColor *textColor;
@property (nonatomic) BOOL displaysAsynchronously;

@end

NS_ASSUME_NONNULL_END
