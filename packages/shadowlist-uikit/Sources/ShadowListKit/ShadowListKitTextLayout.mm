#import <ShadowListKit/ShadowListKitText.h>
#import "Internal/ShadowListKitPlatform.h"

#import <CoreText/CoreText.h>

#include <vector>

#pragma mark - Layout

static NSString *const ShadowListKitTruncationToken = @"ShadowListKitTruncationToken";

@implementation ShadowListKitTextLayout {
  CFArrayRef _lines;
  std::vector<CGPoint> _origins;
  std::vector<CGFloat> _lineWidths;
  std::vector<CFIndex> _stringOffsets;
  CGFloat _lineHeight;
  CGFloat _baselineShift;
}

+ (instancetype)layoutWithString:(NSString *)string
                            font:(ShadowListKitFont *)font
                      lineHeight:(CGFloat)lineHeight
                            kern:(CGFloat)kern
                           width:(CGFloat)width
                    maximumLines:(NSUInteger)maximumLines
                       alignment:(NSTextAlignment)alignment
{
  return [self layoutWithAttributedString:[[NSAttributedString alloc] initWithString:string]
                                     font:font
                               lineHeight:lineHeight
                                     kern:kern
                                    width:width
                             maximumLines:maximumLines
                                alignment:alignment];
}

+ (instancetype)layoutWithAttributedString:(NSAttributedString *)string
                                      font:(ShadowListKitFont *)font
                                lineHeight:(CGFloat)lineHeight
                                      kern:(CGFloat)kern
                                     width:(CGFloat)width
                              maximumLines:(NSUInteger)maximumLines
                                 alignment:(NSTextAlignment)alignment
{
  return [[self alloc] initWithAttributedString:string
                                           font:font
                                     lineHeight:lineHeight
                                           kern:kern
                                          width:width
                                   maximumLines:maximumLines
                                      alignment:alignment];
}

- (instancetype)initWithAttributedString:(NSAttributedString *)string
                                    font:(ShadowListKitFont *)font
                              lineHeight:(CGFloat)lineHeight
                                    kern:(CGFloat)kern
                                   width:(CGFloat)width
                            maximumLines:(NSUInteger)maximumLines
                               alignment:(NSTextAlignment)alignment
{
  if (!(self = [super init])) {
    return nil;
  }
  _lineHeight = lineHeight;
  // Center the glyphs in the taller line, the same shift the labels use.
#if TARGET_OS_OSX
  CTFontRef ctFont = (__bridge CTFontRef)font;
  CGFloat fontLineHeight = CTFontGetAscent(ctFont) + CTFontGetDescent(ctFont) + CTFontGetLeading(ctFont);
  _baselineShift = (lineHeight - fontLineHeight) / 4;
#else
  _baselineShift = (lineHeight - font.lineHeight) / 4;
#endif

  CTTextAlignment ctAlignment = alignment == NSTextAlignmentRight    ? kCTTextAlignmentRight
                                : alignment == NSTextAlignmentCenter ? kCTTextAlignmentCenter
                                                                     : kCTTextAlignmentNatural;
  CGFloat fixedHeight = lineHeight;
  CTLineBreakMode breakMode = kCTLineBreakByWordWrapping;
  CTParagraphStyleSetting settings[] = {
    {kCTParagraphStyleSpecifierAlignment, sizeof(ctAlignment), &ctAlignment},
    {kCTParagraphStyleSpecifierMinimumLineHeight, sizeof(fixedHeight), &fixedHeight},
    {kCTParagraphStyleSpecifierMaximumLineHeight, sizeof(fixedHeight), &fixedHeight},
    {kCTParagraphStyleSpecifierLineBreakMode, sizeof(breakMode), &breakMode},
  };
  CTParagraphStyleRef paragraph = CTParagraphStyleCreate(settings, sizeof(settings) / sizeof(settings[0]));
  NSDictionary *attributes = @{
    (id)kCTFontAttributeName : font,
    (id)kCTKernAttributeName : @(kern),
    (id)kCTParagraphStyleAttributeName : (__bridge id)paragraph,
    (id)kCTForegroundColorFromContextAttributeName : @YES,
  };
  NSMutableAttributedString *text = [[NSMutableAttributedString alloc] initWithString:string.string
                                                                           attributes:attributes];
  [string enumerateAttributesInRange:NSMakeRange(0, string.length)
                             options:0
                          usingBlock:^(NSDictionary<NSAttributedStringKey, id> *run, NSRange range, BOOL *) {
                            [text addAttributes:run range:range];
                            if (run[NSForegroundColorAttributeName] || run[(id)kCTForegroundColorAttributeName]) {
                              [text addAttribute:(id)kCTForegroundColorFromContextAttributeName value:@NO range:range];
                            }
                          }];
  // Every run uses the layout's line height and alignment, including supplied rich text.
  [text addAttribute:(id)kCTParagraphStyleAttributeName value:(__bridge id)paragraph range:NSMakeRange(0, text.length)];
  CFRelease(paragraph);

  CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString((__bridge CFAttributedStringRef)text);
  CGFloat frameHeight = 100000;
  CGPathRef path = CGPathCreateWithRect(CGRectMake(0, 0, width, frameHeight), NULL);
  CTFrameRef frame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), path, NULL);
  CFArrayRef lines = CTFrameGetLines(frame);
  CFIndex count = CFArrayGetCount(lines);
  std::vector<CGPoint> origins((size_t)count);
  if (count > 0) {
    CTFrameGetLineOrigins(frame, CFRangeMake(0, 0), origins.data());
  }

  CFIndex kept = maximumLines > 0 ? MIN(count, (CFIndex)maximumLines) : count;
  CFMutableArrayRef keptLines = CFArrayCreateMutable(kCFAllocatorDefault, kept, &kCFTypeArrayCallBacks);
  _stringOffsets.resize((size_t)kept);
  for (CFIndex index = 0; index < kept; ++index) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(lines, index);
    if (index == kept - 1 && kept < count) {
      // The last line shown takes the rest of the text and ends in an ellipsis.
      CFRange range = CTLineGetStringRange(line);
      _stringOffsets[(size_t)index] = range.location;
      NSAttributedString *rest = [text
        attributedSubstringFromRange:NSMakeRange((NSUInteger)range.location, text.length - (NSUInteger)range.location)];
      CTLineRef full = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)rest);
      NSMutableDictionary *tokenAttributes = [attributes mutableCopy];
      tokenAttributes[ShadowListKitTruncationToken] = @YES;
      NSAttributedString *ellipsis = [[NSAttributedString alloc] initWithString:@"…" attributes:tokenAttributes];
      CTLineRef token = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)ellipsis);
      CTLineRef truncated = CTLineCreateTruncatedLine(full, width, kCTLineTruncationEnd, token);
      CFArrayAppendValue(keptLines, truncated ?: full);
      if (truncated) {
        CFRelease(truncated);
      }
      CFRelease(full);
      CFRelease(token);
    } else {
      CFArrayAppendValue(keptLines, line);
    }
  }
  _lines = keptLines;
  _origins.resize((size_t)kept);
  _lineWidths.resize((size_t)kept);
  CGFloat widest = 0;
  for (CFIndex index = 0; index < kept; ++index) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(_lines, index);
    // Origins come bottom up from the top of the frame. Keep the baseline as distance from the top.
    _origins[(size_t)index] = CGPointMake(origins[(size_t)index].x, frameHeight - origins[(size_t)index].y);
    CGFloat lineWidth =
      (CGFloat)CTLineGetTypographicBounds(line, NULL, NULL, NULL) - (CGFloat)CTLineGetTrailingWhitespaceWidth(line);
    _lineWidths[(size_t)index] = MAX(0, lineWidth);
    widest = MAX(widest, lineWidth);
  }
  _lineCount = (NSUInteger)kept;
  _size = CGSizeMake(MIN(width, ceil(widest)), lineHeight * kept);

  CFRelease(frame);
  CGPathRelease(path);
  CFRelease(framesetter);
  return self;
}

- (void)dealloc
{
  if (_lines) {
    CFRelease(_lines);
  }
}

- (CGRect)rectForLineAtIndex:(NSUInteger)index
{
  if (index >= _lineCount) {
    return CGRectNull;
  }
  return CGRectMake(_origins[index].x, _lineHeight * index, _lineWidths[index], _lineHeight);
}

/*
 * The existing typeset line maps a touch back to the attributed string's UTF-16 index.
 */
- (NSUInteger)characterIndexAtPoint:(CGPoint)point
{
  if (_lineHeight <= 0 || point.y < 0 || point.y >= _size.height) {
    return NSNotFound;
  }
  NSUInteger index = (NSUInteger)(point.y / _lineHeight);
  if (!CGRectContainsPoint([self rectForLineAtIndex:index], point)) {
    return NSNotFound;
  }
  CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(_lines, (CFIndex)index);
  CGFloat x = point.x - _origins[index].x;
  CFArrayRef runs = CTLineGetGlyphRuns(line);
  for (CFIndex at = 0; at < CFArrayGetCount(runs); ++at) {
    CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(runs, at);
    NSDictionary *attributes = (__bridge NSDictionary *)CTRunGetAttributes(run);
    if (!attributes[ShadowListKitTruncationToken] || CTRunGetGlyphCount(run) == 0) {
      continue;
    }
    CGPoint position;
    CTRunGetPositions(run, CFRangeMake(0, 1), &position);
    CGFloat width = (CGFloat)CTRunGetTypographicBounds(run, CFRangeMake(0, 0), NULL, NULL, NULL);
    if (x >= position.x && x <= position.x + width) {
      return NSNotFound;
    }
  }
  CFIndex character = CTLineGetStringIndexForPosition(line, CGPointMake(x, 0));
  return character == kCFNotFound ? NSNotFound : (NSUInteger)(character + _stringOffsets[index]);
}

- (void)drawAtPoint:(CGPoint)point inContext:(CGContextRef)context color:(CGColorRef)color
{
  CGContextSaveGState(context);
  CGContextSetFillColorWithColor(context, color);
  // The context runs top down. Flip the glyphs, not the positions.
  CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1, -1));
  CFIndex count = CFArrayGetCount(_lines);
  for (CFIndex index = 0; index < count; ++index) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(_lines, index);
    CGPoint origin = _origins[(size_t)index];
    CGContextSetTextPosition(context, point.x + origin.x, point.y + origin.y - _baselineShift);
    CTLineDraw(line, context);
  }
  CGContextRestoreGState(context);
}

@end
