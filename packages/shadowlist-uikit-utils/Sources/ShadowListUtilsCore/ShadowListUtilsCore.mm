#import "include/ShadowListUtilsCore/ShadowListUtilsCore.h"

#include <shadowlist-core/host/RowSizeSpec.hpp>
#include <shadowlist-core/utils/Avatar.hpp>
#include <shadowlist-core/utils/BoardColumnLayout.hpp>
#include <shadowlist-core/utils/ChatLayout.hpp>
#include <shadowlist-core/utils/ContactSection.hpp>
#include <shadowlist-core/utils/FeedImageFrame.hpp>
#include <shadowlist-core/utils/PollTally.hpp>
#include <shadowlist-core/utils/Progress.hpp>
#include <shadowlist-core/utils/MarkdownDocument.hpp>
#include <shadowlist-core/utils/Text.hpp>
#include <shadowlist-core/utils/RelativeTime.hpp>
#include <shadowlist-core/utils/Text.hpp>

#include <string>
#include <vector>

using namespace azimgd::shadowlist;

const CGFloat SHADOWLIST_UTILS_CHAT_BUBBLE_PADDING_HORIZONTAL = CHAT_BUBBLE_PADDING_HORIZONTAL;
const CGFloat SHADOWLIST_UTILS_CHAT_BUBBLE_WIDTH_FRACTION = CHAT_BUBBLE_WIDTH_FRACTION;
const CGFloat SHADOWLIST_UTILS_FEED_SINGLE_IMAGE_HEIGHT = FEED_SINGLE_IMAGE_HEIGHT;
const CGFloat SHADOWLIST_UTILS_ASSISTANT_USER_BUBBLE_WIDTH_FRACTION = ASSISTANT_USER_BUBBLE_WIDTH_FRACTION;
const CGFloat SHADOWLIST_UTILS_ASSISTANT_USER_BUBBLE_PADDING_HORIZONTAL = ASSISTANT_USER_BUBBLE_PADDING_HORIZONTAL;
const CGFloat SHADOWLIST_UTILS_ASSISTANT_USER_BUBBLE_PADDING_VERTICAL = ASSISTANT_USER_BUBBLE_PADDING_VERTICAL;

/*
 * An NSString as standard UTF-8, read unit by unit: a surrogate pair is one code point, and
 * half a pair is U+FFFD. UTF8String gives up on half a pair and returns nothing.
 */
static std::string readString(NSString *value)
{
  NSUInteger length = value.length;
  std::vector<unichar> units(length);
  [value getCharacters:units.data() range:NSMakeRange(0, length)];
  std::string result;
  result.reserve(length);
  for (NSUInteger at = 0; at < length; ++at) {
    char32_t codePoint = units[at];
    bool high = codePoint >= 0xD800 && codePoint <= 0xDBFF;
    if (high && at + 1 < length && units[at + 1] >= 0xDC00 && units[at + 1] <= 0xDFFF) {
      codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (units[at + 1] - 0xDC00);
      ++at;
    }
    appendUtf8(result, codePoint);
  }
  return result;
}

/*
 * Standard UTF-8 from the core as an NSString.
 */
static NSString *makeString(const std::string& text)
{
  std::vector<unichar> units;
  units.reserve(text.size());
  std::size_t offset = 0;
  while (offset < text.size()) {
    char32_t codePoint = decodeUtf8(text, offset);
    if (codePoint < 0x10000) {
      units.push_back((unichar)codePoint);
      continue;
    }
    char32_t astral = codePoint - 0x10000;
    units.push_back((unichar)(0xD800 + (astral >> 10)));
    units.push_back((unichar)(0xDC00 + (astral & 0x3FF)));
  }
  return [NSString stringWithCharacters:units.data() length:units.size()];
}

@implementation ShadowListUtilsCore

#pragma mark - Avatar

+ (NSInteger)avatarColorIndexForName:(NSString *)name paletteSize:(NSInteger)paletteSize
{
  if (paletteSize <= 0) {
    return 0;
  }
  return (NSInteger)avatarColorIndex(readString(name), (std::size_t)paletteSize);
}

+ (NSString *)avatarInitialsForName:(NSString *)name
{
  return makeString(avatarInitials(readString(name)));
}

+ (ShadowListUtilsAvatarInitialsMetrics)avatarInitialsMetricsForSize:(CGFloat)size
{
  AvatarInitialsMetrics metrics = avatarInitialsMetrics(size);
  return (ShadowListUtilsAvatarInitialsMetrics){.fontSize = (CGFloat)metrics.fontSize, .lineHeight = (CGFloat)metrics.lineHeight};
}

#pragma mark - Relative time

+ (ShadowListUtilsRelativeTime)relativeTimeForDate:(NSTimeInterval)dateMs now:(NSTimeInterval)nowMs
{
  RelativeTime time = relativeTime(dateMs, nowMs);
  ShadowListUtilsRelativeTime result;
  result.count = (NSInteger)time.count;
  switch (time.unit) {
    case RelativeTimeUnit::Now:
      result.unit = ShadowListUtilsRelativeTimeUnitNow;
      break;
    case RelativeTimeUnit::Minutes:
      result.unit = ShadowListUtilsRelativeTimeUnitMinutes;
      break;
    case RelativeTimeUnit::Hours:
      result.unit = ShadowListUtilsRelativeTimeUnitHours;
      break;
    case RelativeTimeUnit::Days:
      result.unit = ShadowListUtilsRelativeTimeUnitDays;
      break;
    case RelativeTimeUnit::Date:
      result.unit = ShadowListUtilsRelativeTimeUnitDate;
      break;
  }
  return result;
}

+ (BOOL)relativeTimeHasDate:(NSTimeInterval)dateMs
{
  return relativeTimeHasDate(dateMs);
}

#pragma mark - Feed

+ (ShadowListUtilsFeedImageFrame)feedSingleImageFrameForWidth:(CGFloat)width height:(CGFloat)height
{
  FeedImageFrame frame = feedSingleImageFrame(width, height);
  return (ShadowListUtilsFeedImageFrame){.aspectRatio = (CGFloat)frame.aspectRatio, .height = (CGFloat)frame.height};
}

#pragma mark - Chat

+ (BOOL)chatMessageSizeSpec:(ShadowListUtilsChatSizeSpec *)spec
                      shape:(ShadowListUtilsChatShape)shape
                    metrics:(ShadowListUtilsChatMetrics)metrics
{
  ChatMessageShape coreShape;
  coreShape.hasText = shape.hasText;
  coreShape.hasImages = shape.hasImages;
  coreShape.isOwn = shape.isOwn;
  coreShape.showsAuthor = shape.showsAuthor;
  coreShape.hasCaption = shape.hasCaption;
  coreShape.failed = shape.failed;

  ChatBubbleMetrics coreMetrics;
  coreMetrics.spacingXxs = metrics.spacingXxs;
  coreMetrics.spacingXs = metrics.spacingXs;
  coreMetrics.spacingSm = metrics.spacingSm;
  coreMetrics.spacingMd = metrics.spacingMd;
  coreMetrics.spacingLg = metrics.spacingLg;
  coreMetrics.bodyFontSize = metrics.bodyFontSize;
  coreMetrics.bodyLineHeight = metrics.bodyLineHeight;
  coreMetrics.bodyLetterSpacing = metrics.bodyLetterSpacing;
  coreMetrics.captionLineHeight = metrics.captionLineHeight;

  RowSizeSpec coreSpec;
  if (!chatMessageSizeSpec(coreShape, coreMetrics, coreSpec)) {
    return NO;
  }
  spec->insetWidth = (CGFloat)coreSpec.insetWidth;
  spec->insetHeight = (CGFloat)coreSpec.insetHeight;
  spec->widthFraction = (CGFloat)coreSpec.widthFraction;
  return YES;
}

#pragma mark - Contacts

+ (NSString *)sectionInitialForName:(NSString *)name
{
  return makeString(sectionInitial(readString(name)));
}

+ (NSString *)sectionOtherTitle
{
  return makeString(std::string(SECTION_OTHER_TITLE));
}

#pragma mark - Poll

+ (ShadowListUtilsPollTally)pollTallyForVotes:(NSArray<NSNumber *> *)votes
{
  std::vector<double> counts;
  counts.reserve(votes.count);
  for (NSNumber *vote in votes) {
    counts.push_back(vote.doubleValue);
  }
  PollTally tally = pollTally(counts);
  return (ShadowListUtilsPollTally){.totalVotes = tally.totalVotes,
    .leadingIndex = tally.leadingIndex == UNDEFINED_INDEX ? NSNotFound : (NSInteger)tally.leadingIndex};
}

+ (NSInteger)pollPercentForVotes:(double)votes totalVotes:(double)totalVotes
{
  return pollPercent(votes, totalVotes);
}

#pragma mark - Board

+ (double)clampFraction:(double)fraction
{
  return azimgd::shadowlist::clampFraction(fraction);
}

+ (NSInteger)progressPercent:(double)fraction
{
  return azimgd::shadowlist::progressPercent(fraction);
}

+ (ShadowListUtilsBoardColumnLayout)boardColumnLayoutForWidth:(CGFloat)boardWidth columnWidth:(CGFloat)columnWidth sidePadding:(CGFloat)sidePadding
{
  BoardColumnLayout layout = boardColumnLayout(boardWidth, columnWidth, sidePadding);
  return (ShadowListUtilsBoardColumnLayout){.columnWidth = layout.columnWidth, .sidePadding = layout.sidePadding, .snapInterval = layout.snapInterval};
}

+ (CGFloat)boardColumnHeightForHeights:(NSArray<NSNumber *> *)heights minimum:(CGFloat)minimum maximum:(CGFloat)maximum
{
  std::vector<double> values;
  values.reserve(heights.count);
  for (NSNumber *height in heights) values.push_back(height.doubleValue);
  return boardColumnHeight(values, minimum, maximum);
}

+ (BOOL)assistantUserMessageSizeSpec:(ShadowListUtilsChatSizeSpec *)spec
                           hasText:(BOOL)hasText
                    hasAttachments:(BOOL)hasAttachments
                           metrics:(ShadowListUtilsChatMetrics)metrics
{
  ChatBubbleMetrics coreMetrics;
  coreMetrics.spacingXxs = metrics.spacingXxs;
  coreMetrics.spacingXs = metrics.spacingXs;
  coreMetrics.spacingSm = metrics.spacingSm;
  coreMetrics.spacingMd = metrics.spacingMd;
  coreMetrics.spacingLg = metrics.spacingLg;
  coreMetrics.bodyFontSize = metrics.bodyFontSize;
  coreMetrics.bodyLineHeight = metrics.bodyLineHeight;
  coreMetrics.bodyLetterSpacing = metrics.bodyLetterSpacing;
  coreMetrics.captionLineHeight = metrics.captionLineHeight;

  RowSizeSpec coreSpec;
  if (!assistantUserMessageSizeSpec(hasText, hasAttachments, coreMetrics, coreSpec)) {
    return NO;
  }
  spec->insetWidth = (CGFloat)coreSpec.insetWidth;
  spec->insetHeight = (CGFloat)coreSpec.insetHeight;
  spec->widthFraction = (CGFloat)coreSpec.widthFraction;
  return YES;
}

+ (NSArray<ShadowListUtilsAssistantStep *> *)stepsOf:(NSString *)content positions:(NSArray<NSNumber *> *)positions
{
  return ShadowListUtilsStepsOf(content, positions);
}

+ (NSArray<ShadowListUtilsMarkdownBlock *> *)parseMarkdown:(NSString *)source
{
  return ShadowListUtilsParseMarkdown(source);
}

+ (BOOL)isSafeUrl:(NSString *)url
{
  NSData *utf8 = [url dataUsingEncoding:NSUTF8StringEncoding];
  return isSafeUrl(std::string_view(static_cast<const char *>(utf8.bytes), utf8.length));
}

+ (NSString *)trim:(NSString *)text
{
  NSData *utf8 = [text dataUsingEncoding:NSUTF8StringEncoding];
  const auto result = trim(std::string_view(static_cast<const char *>(utf8.bytes), utf8.length));
  return [[NSString alloc] initWithBytes:result.data() length:result.size() encoding:NSUTF8StringEncoding];
}

@end
