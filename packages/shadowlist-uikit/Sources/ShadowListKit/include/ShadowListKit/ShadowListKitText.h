#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Text typeset ahead of time. Creating one is thread safe and does all the line breaking. A
 * row's layout can be computed off the main thread, and the main thread only draws the lines.
 * Every line is lineHeight tall, like a label with a fixed minimum and maximum line height.
 * size is the width of the widest line, rounded up, and lineCount lines of lineHeight.
 * drawAtPoint:inContext:color: draws with the top left corner at point, in a context with
 * UIKit's flipped coordinates.
 */
NS_SWIFT_SENDABLE
@interface ShadowListKitTextLayout : NSObject

+ (instancetype)layoutWithString:(NSString *)string
                            font:(UIFont *)font
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
 * With displaysAsynchronously the lines are drawn into a bitmap on a background queue and the
 * view shows it when ready, leaving the main thread only the hand over of the bitmap.
 */
@interface ShadowListKitTextView : UIView

@property (nonatomic, strong, nullable) ShadowListKitTextLayout *textLayout;
@property (nonatomic, strong) UIColor *textColor;
@property (nonatomic) BOOL displaysAsynchronously;

@end

NS_ASSUME_NONNULL_END
