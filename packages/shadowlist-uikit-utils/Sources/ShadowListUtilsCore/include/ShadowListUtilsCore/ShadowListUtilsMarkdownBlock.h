#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * The core's MarkdownInlineStyle, in the same order. These are parser results, not a second grammar.
 */
typedef NS_ENUM(NSInteger, ShadowListUtilsMarkdownInlineStyle) {
  ShadowListUtilsMarkdownInlineStyleText,
  ShadowListUtilsMarkdownInlineStyleBold,
  ShadowListUtilsMarkdownInlineStyleItalic,
  ShadowListUtilsMarkdownInlineStyleCode,
  ShadowListUtilsMarkdownInlineStyleLink,
};

/*
 * One run of parsed text. A link's href is absent while its target is still streaming.
 */
@interface ShadowListUtilsMarkdownInline : NSObject
@property (nonatomic, readonly) ShadowListUtilsMarkdownInlineStyle style;
@property (nonatomic, copy, readonly) NSString *text;
@property (nonatomic, copy, readonly, nullable) NSString *href;
@end

/*
 * The core's MarkdownBlockType, in the same order.
 */
typedef NS_ENUM(NSInteger, ShadowListUtilsMarkdownBlockType) {
  ShadowListUtilsMarkdownBlockTypeHeading,
  ShadowListUtilsMarkdownBlockTypeParagraph,
  ShadowListUtilsMarkdownBlockTypeQuote,
  ShadowListUtilsMarkdownBlockTypeList,
  ShadowListUtilsMarkdownBlockTypeCode,
  ShadowListUtilsMarkdownBlockTypeTable,
  ShadowListUtilsMarkdownBlockTypeRule,
};

/*
 * A typed copy of one core Markdown block. Runs are grouped by list item or table cell.
 */
@interface ShadowListUtilsMarkdownBlock : NSObject
@property (nonatomic, readonly) ShadowListUtilsMarkdownBlockType type;
@property (nonatomic, copy, readonly) NSString *key;
@property (nonatomic, copy, readonly) NSString *raw;
@property (nonatomic, copy, readonly) NSArray<NSArray<ShadowListUtilsMarkdownInline *> *> *runs;
@property (nonatomic, copy, readonly) NSArray<NSNumber *> *rowCellCounts;
@property (nonatomic, readonly) NSInteger level;
@property (nonatomic, readonly) BOOL ordered;
@property (nonatomic, readonly) double start;
@property (nonatomic, copy, readonly) NSString *language;
@property (nonatomic, copy, readonly) NSString *code;
@property (nonatomic, readonly) BOOL closed;
@end

FOUNDATION_EXPORT NSArray<ShadowListUtilsMarkdownBlock *> *ShadowListUtilsParseMarkdown(NSString *source);

NS_ASSUME_NONNULL_END
