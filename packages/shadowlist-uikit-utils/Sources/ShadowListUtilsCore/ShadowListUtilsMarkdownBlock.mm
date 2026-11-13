#import "include/ShadowListUtilsCore/ShadowListUtilsMarkdownBlock.h"

#include <shadowlist-core/utils/MarkdownDocument.hpp>

/*
 * These writable properties only construct typed copies of the shared parser's result.
 */
@interface ShadowListUtilsMarkdownInline ()
@property (nonatomic) ShadowListUtilsMarkdownInlineStyle style;
@property (nonatomic, copy) NSString *text;
@property (nonatomic, copy, nullable) NSString *href;
@end

@implementation ShadowListUtilsMarkdownInline
@end

@interface ShadowListUtilsMarkdownBlock ()
@property (nonatomic) ShadowListUtilsMarkdownBlockType type;
@property (nonatomic, copy) NSString *key;
@property (nonatomic, copy) NSString *raw;
@property (nonatomic, copy) NSArray<NSArray<ShadowListUtilsMarkdownInline *> *> *runs;
@property (nonatomic, copy) NSArray<NSNumber *> *rowCellCounts;
@property (nonatomic) NSInteger level;
@property (nonatomic) BOOL ordered;
@property (nonatomic) double start;
@property (nonatomic, copy) NSString *language;
@property (nonatomic, copy) NSString *code;
@property (nonatomic) BOOL closed;
@end

@implementation ShadowListUtilsMarkdownBlock
@end

namespace {

NSString *makeString(const std::string& text) {
  return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
}

}

NSArray<ShadowListUtilsMarkdownBlock *> *ShadowListUtilsParseMarkdown(NSString *source)
{
  namespace sl = azimgd::shadowlist;
  NSData *utf8 = [source dataUsingEncoding:NSUTF8StringEncoding];
  auto blocks = sl::parseMarkdown(std::string_view(static_cast<const char *>(utf8.bytes), utf8.length));
  NSMutableArray<ShadowListUtilsMarkdownBlock *> *result = [NSMutableArray arrayWithCapacity:blocks.size()];
  for (const auto& block : blocks) {
    ShadowListUtilsMarkdownBlock *copy = [ShadowListUtilsMarkdownBlock new];
    copy.type = static_cast<ShadowListUtilsMarkdownBlockType>(block.type);
    copy.key = makeString(block.key);
    copy.raw = makeString(block.raw);
    NSMutableArray *runs = [NSMutableArray arrayWithCapacity:block.runs.size()];
    for (const auto& row : block.runs) {
      NSMutableArray *inlines = [NSMutableArray arrayWithCapacity:row.size()];
      for (const auto& run : row) {
        ShadowListUtilsMarkdownInline *inlineCopy = [ShadowListUtilsMarkdownInline new];
        inlineCopy.style = static_cast<ShadowListUtilsMarkdownInlineStyle>(run.style);
        inlineCopy.text = makeString(run.text);
        inlineCopy.href = run.hasHref ? makeString(run.href) : nil;
        [inlines addObject:inlineCopy];
      }
      [runs addObject:inlines];
    }
    copy.runs = runs;
    NSMutableArray *counts = [NSMutableArray arrayWithCapacity:block.rowCellCounts.size()];
    for (auto count : block.rowCellCounts) [counts addObject:@(count)];
    copy.rowCellCounts = counts;
    copy.level = block.level;
    copy.ordered = block.ordered;
    copy.start = block.start;
    copy.language = makeString(block.language);
    copy.code = makeString(block.code);
    copy.closed = block.closed;
    [result addObject:copy];
  }
  return result;
}
