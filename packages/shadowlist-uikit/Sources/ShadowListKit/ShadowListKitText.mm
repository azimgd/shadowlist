#import "ShadowListKitText.h"

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
                            font:(UIFont *)font
                      lineHeight:(CGFloat)lineHeight
                            kern:(CGFloat)kern
                           width:(CGFloat)width
                    maximumLines:(NSUInteger)maximumLines
                       alignment:(NSTextAlignment)alignment
{
  return [[self alloc] initWithString:string font:font lineHeight:lineHeight kern:kern width:width maximumLines:maximumLines alignment:alignment];
}

- (instancetype)initWithString:(NSString *)string
                          font:(UIFont *)font
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
  _baselineShift = (lineHeight - font.lineHeight) / 4;

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
  // UIKit contexts run top down. Flip the glyphs, not the positions.
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

#pragma mark - View

static dispatch_queue_t ShadowListKitTextRenderQueue(void)
{
  static dispatch_queue_t queue;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    queue = dispatch_queue_create("slk.text.render", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT, QOS_CLASS_USER_INITIATED, 0));
  });
  return queue;
}

/*
 * Async drawing stats for benchmarks: how many bitmaps were drawn, and how many arrived while
 * their view was already on screen, with how long those waited.
 */
static NSUInteger ShadowListKitAsyncRendered;
static NSUInteger ShadowListKitAsyncLate;
static double ShadowListKitAsyncLateWaitSum;
static double ShadowListKitAsyncLateWaitMax;

@implementation ShadowListKitTextView {
  NSUInteger _generation;
}

+ (NSDictionary *)shadowListKit_asyncStats
{
  return @{
    @"rendered" : @(ShadowListKitAsyncRendered),
    @"late" : @(ShadowListKitAsyncLate),
    @"lateWaitAvgMs" : @(ShadowListKitAsyncLate > 0 ? ShadowListKitAsyncLateWaitSum * 1000 / ShadowListKitAsyncLate : 0),
    @"lateWaitMaxMs" : @(ShadowListKitAsyncLateWaitMax * 1000),
  };
}

+ (void)shadowListKit_resetAsyncStats
{
  ShadowListKitAsyncRendered = 0;
  ShadowListKitAsyncLate = 0;
  ShadowListKitAsyncLateWaitSum = 0;
  ShadowListKitAsyncLateWaitMax = 0;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    self.opaque = NO;
    self.backgroundColor = UIColor.clearColor;
    self.contentMode = UIViewContentModeTopLeft;
    self.userInteractionEnabled = NO;
    _textColor = UIColor.labelColor;
  }
  return self;
}

- (void)setTextLayout:(ShadowListKitTextLayout *)textLayout
{
  if (_textLayout == textLayout) {
    return;
  }
  _textLayout = textLayout;
  [self redisplay];
}

- (void)setTextColor:(UIColor *)textColor
{
  if ([_textColor isEqual:textColor]) {
    return;
  }
  _textColor = textColor;
  [self redisplay];
}

- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection
{
  [super traitCollectionDidChange:previousTraitCollection];
  if ([self.traitCollection hasDifferentColorAppearanceComparedToTraitCollection:previousTraitCollection]) {
    [self redisplay];
  }
}

- (void)redisplay
{
  ++_generation;
  if (!_displaysAsynchronously) {
    [self setNeedsDisplay];
    return;
  }
  ShadowListKitTextLayout *layout = _textLayout;
  if (!layout || layout.lineCount == 0) {
    self.layer.contents = nil;
    return;
  }
  NSUInteger generation = _generation;
  CGColorRef color = CGColorRetain([_textColor resolvedColorWithTraitCollection:self.traitCollection].CGColor);
  CGFloat scale = self.window.screen.scale ?: UIScreen.mainScreen.scale;
  CGSize size = layout.size;
  self.layer.contents = nil;
  CFTimeInterval requested = CACurrentMediaTime();
  __weak ShadowListKitTextView *weakSelf = self;
  dispatch_async(ShadowListKitTextRenderQueue(), ^{
    UIGraphicsImageRendererFormat *format = [UIGraphicsImageRendererFormat preferredFormat];
    format.scale = scale;
    format.opaque = NO;
    UIGraphicsImageRenderer *renderer = [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];
    UIImage *image = [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
      [layout drawAtPoint:CGPointZero inContext:context.CGContext color:color];
    }];
    CGColorRelease(color);
    dispatch_async(dispatch_get_main_queue(), ^{
      ShadowListKitTextView *view = weakSelf;
      if (view && view->_generation == generation) {
        view.layer.contentsScale = scale;
        view.layer.contents = (__bridge id)image.CGImage;
        ++ShadowListKitAsyncRendered;
        UIWindow *window = view.window;
        if (window && !view.hidden && CGRectIntersectsRect([view convertRect:view.bounds toView:nil], window.bounds)) {
          double wait = CACurrentMediaTime() - requested;
          ++ShadowListKitAsyncLate;
          ShadowListKitAsyncLateWaitSum += wait;
          ShadowListKitAsyncLateWaitMax = MAX(ShadowListKitAsyncLateWaitMax, wait);
        }
      }
    });
  });
}

- (void)drawRect:(CGRect)rect
{
  if (_displaysAsynchronously || !_textLayout) {
    return;
  }
  [_textLayout drawAtPoint:CGPointZero inContext:UIGraphicsGetCurrentContext() color:_textColor.CGColor];
}

@end
