#pragma once

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import "ShadowListUtilsMarkdownBlock.h"
#import "ShadowListUtilsAssistantStep.h"
#import "ShadowListUtilsWrapLayout.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Template constants of the core, in points: a chat bubble's padding on each side and its
 * share of the row's width, and the height of a feed post's single image that has no size.
 */
FOUNDATION_EXPORT const CGFloat SHADOWLIST_UTILS_CHAT_BUBBLE_PADDING_HORIZONTAL;
FOUNDATION_EXPORT const CGFloat SHADOWLIST_UTILS_CHAT_BUBBLE_WIDTH_FRACTION;
FOUNDATION_EXPORT const CGFloat SHADOWLIST_UTILS_FEED_SINGLE_IMAGE_HEIGHT;
FOUNDATION_EXPORT const CGFloat SHADOWLIST_UTILS_ASSISTANT_USER_BUBBLE_WIDTH_FRACTION;
FOUNDATION_EXPORT const CGFloat SHADOWLIST_UTILS_ASSISTANT_USER_BUBBLE_PADDING_HORIZONTAL;
FOUNDATION_EXPORT const CGFloat SHADOWLIST_UTILS_ASSISTANT_USER_BUBBLE_PADDING_VERTICAL;

/*
 * The bucket a moment falls in. The order matches the core's RelativeTimeUnit.
 */
typedef NS_ENUM(NSInteger, ShadowListUtilsRelativeTimeUnit) {
  ShadowListUtilsRelativeTimeUnitNow,
  ShadowListUtilsRelativeTimeUnitMinutes,
  ShadowListUtilsRelativeTimeUnitHours,
  ShadowListUtilsRelativeTimeUnitDays,
  ShadowListUtilsRelativeTimeUnitDate,
};

/*
 * How long ago a moment was. count is the whole minutes, hours or days, and 0 for Now and Date.
 */
typedef struct {
  ShadowListUtilsRelativeTimeUnit unit;
  NSInteger count;
} ShadowListUtilsRelativeTime;

/*
 * How an avatar's initials are typeset, in the units of the avatar's size.
 */
typedef struct {
  CGFloat fontSize;
  CGFloat lineHeight;
} ShadowListUtilsAvatarInitialsMetrics;

/*
 * How an image is framed: by aspectRatio, width over height, or by a fixed height. The one that
 * does not apply is 0.
 */
typedef struct {
  CGFloat aspectRatio;
  CGFloat height;
} ShadowListUtilsFeedImageFrame;

/*
 * A board's columns in points, from the shared layout helper.
 */
typedef struct {
  CGFloat columnWidth;
  CGFloat sidePadding;
  CGFloat snapInterval;
} ShadowListUtilsBoardColumnLayout;

/*
 * What of a chat message changes its row's height. The fields are the core's ChatMessageShape.
 */
typedef struct {
  BOOL hasText;
  BOOL hasImages;
  BOOL isOwn;
  BOOL showsAuthor;
  BOOL hasCaption;
  BOOL failed;
} ShadowListUtilsChatShape;

/*
 * The theme numbers a chat bubble is laid out from, in points. The fields are the core's
 * ChatBubbleMetrics.
 */
typedef struct {
  CGFloat spacingXxs;
  CGFloat spacingXs;
  CGFloat spacingSm;
  CGFloat spacingMd;
  CGFloat spacingLg;
  CGFloat bodyFontSize;
  CGFloat bodyLineHeight;
  CGFloat bodyLetterSpacing;
  CGFloat captionLineHeight;
} ShadowListUtilsChatMetrics;

/*
 * The space of a chat row around its text. insetWidth is taken off the wrap width after
 * widthFraction of the row's width, and insetHeight is added to the height of the text.
 */
typedef struct {
  CGFloat insetWidth;
  CGFloat insetHeight;
  CGFloat widthFraction;
} ShadowListUtilsChatSizeSpec;

/*
 * A poll's total and leading option. NSNotFound means there are no options.
 */
typedef struct {
  double totalVotes;
  NSInteger leadingIndex;
} ShadowListUtilsPollTally;

/*
 * The template logic of the shadowlist core, in packages/shadowlist-core/utils. Every method
 * is stateless and safe to call from any thread. A string crosses as UTF-8, and half a
 * surrogate pair in it crosses as U+FFFD.
 *
 * - avatarColorIndexForName:paletteSize: the palette slot of a name, the same on every platform.
 * - avatarInitialsForName: the first character of the name's first word and of its last word
 *   when it has more than one. The case is the name's.
 * - avatarInitialsMetricsForSize: the font size and the line box of the initials in an avatar
 *   of a size.
 * - relativeTimeForDate:now: how long ago dateMs was at nowMs, both in milliseconds.
 * - relativeTimeHasDate: whether dateMs is a moment that can be written as a calendar date.
 * - feedSingleImageFrameForWidth:height: the frame of a feed post's single image.
 * - chatMessageSizeSpec:shape:metrics: fills the spec of a text message and returns YES. Returns
 *   NO for a message with images or without text, which the row measures itself.
 * - sectionInitialForName: the title of the section a contact's name falls in. An ASCII letter
 *   comes back in upper case, and a first character outside ASCII as written.
 * - sectionOtherTitle: the title of the section for names that start with no letter.
 */
NS_SWIFT_SENDABLE
@interface ShadowListUtilsCore : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

+ (NSInteger)avatarColorIndexForName:(NSString *)name paletteSize:(NSInteger)paletteSize;
+ (NSString *)avatarInitialsForName:(NSString *)name;
+ (ShadowListUtilsAvatarInitialsMetrics)avatarInitialsMetricsForSize:(CGFloat)size;
+ (ShadowListUtilsRelativeTime)relativeTimeForDate:(NSTimeInterval)dateMs now:(NSTimeInterval)nowMs;
+ (BOOL)relativeTimeHasDate:(NSTimeInterval)dateMs;
+ (ShadowListUtilsFeedImageFrame)feedSingleImageFrameForWidth:(CGFloat)width height:(CGFloat)height;
+ (BOOL)chatMessageSizeSpec:(ShadowListUtilsChatSizeSpec *)spec
                      shape:(ShadowListUtilsChatShape)shape
                    metrics:(ShadowListUtilsChatMetrics)metrics;
+ (NSString *)sectionInitialForName:(NSString *)name;
+ (NSString *)sectionOtherTitle;
+ (ShadowListUtilsPollTally)pollTallyForVotes:(NSArray<NSNumber *> *)votes;
+ (NSInteger)pollPercentForVotes:(double)votes totalVotes:(double)totalVotes;
+ (ShadowListUtilsBoardColumnLayout)boardColumnLayoutForWidth:(CGFloat)boardWidth columnWidth:(CGFloat)columnWidth sidePadding:(CGFloat)sidePadding;
+ (CGFloat)boardColumnHeightForHeights:(NSArray<NSNumber *> *)heights minimum:(CGFloat)minimum maximum:(CGFloat)maximum;

/*
 * The existing shared Markdown grammar and link policy. Parser results are typed copies.
 */
+ (BOOL)assistantUserMessageSizeSpec:(ShadowListUtilsChatSizeSpec *)spec
                           hasText:(BOOL)hasText
                    hasAttachments:(BOOL)hasAttachments
                           metrics:(ShadowListUtilsChatMetrics)metrics;
+ (NSArray<ShadowListUtilsAssistantStep *> *)stepsOf:(NSString *)content positions:(NSArray<NSNumber *> *)positions NS_SWIFT_NAME(stepsOf(_:positions:));
+ (NSArray<ShadowListUtilsMarkdownBlock *> *)parseMarkdown:(NSString *)source;
+ (BOOL)isSafeUrl:(NSString *)url;
+ (NSString *)trim:(NSString *)text;

@end

NS_ASSUME_NONNULL_END
