#import <ShadowListKit/ShadowListKitText.h>
#import "Internal/ShadowListKitPlatform.h"

#import <CoreText/CoreText.h>

#include <vector>

#pragma mark - Layout

@implementation ShadowListKitTextLayout {
  CFArrayRef _lines;
  std::vector<CGPoint> _origins;
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
  return [[self alloc] initWithString:string font:font lineHeight:lineHeight kern:kern width:width maximumLines:maximumLines alignment:alignment];
}

- (instancetype)initWithString:(NSString *)string
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

  CTTextAlignment ctAlignment = alignment == NSTextAlignmentRight ? kCTTextAlignmentRight
    : alignment == NSTextAlignmentCenter ? kCTTextAlignmentCenter : kCTTextAlignmentNatural;
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
  NSAttributedString *text = [[NSAttributedString alloc] initWithString:string attributes:attributes];
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
  for (CFIndex index = 0; index < kept; ++index) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(lines, index);
    if (index == kept - 1 && kept < count) {
      // The last line shown takes the rest of the text and ends in an ellipsis.
      CFRange range = CTLineGetStringRange(line);
      NSAttributedString *rest = [text attributedSubstringFromRange:NSMakeRange((NSUInteger)range.location, text.length - (NSUInteger)range.location)];
      CTLineRef full = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)rest);
      NSAttributedString *ellipsis = [[NSAttributedString alloc] initWithString:@"…" attributes:attributes];
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
  CGFloat widest = 0;
  for (CFIndex index = 0; index < kept; ++index) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(_lines, index);
    // Origins come bottom up from the top of the frame. Keep the baseline as distance from the top.
    _origins[(size_t)index] = CGPointMake(origins[(size_t)index].x, frameHeight - origins[(size_t)index].y);
    CGFloat lineWidth = (CGFloat)CTLineGetTypographicBounds(line, NULL, NULL, NULL) - (CGFloat)CTLineGetTrailingWhitespaceWidth(line);
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
