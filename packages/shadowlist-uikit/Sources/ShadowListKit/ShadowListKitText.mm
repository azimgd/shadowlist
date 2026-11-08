#import <ShadowListKit/ShadowListKitText.h>

#pragma mark - View

static dispatch_queue_t ShadowListKitTextRenderQueue(void)
{
  static dispatch_queue_t queue;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    queue = dispatch_queue_create("com.shadowlist.kit.text.render", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT, QOS_CLASS_USER_INITIATED, 0));
  });
  return queue;
}

/*
 * Async drawing stats for benchmarks: how many bitmaps were drawn, how many requests a newer
 * one replaced before their bitmap arrived, and how many requests left their view blank on
 * screen, with how long those waited. A request counts as late when its bitmap arrives while
 * the view is on screen, or when the bench's probe saw the view blank before it ended.
 */
static NSUInteger ShadowListKitAsyncRendered;
static NSUInteger ShadowListKitAsyncSuperseded;
static NSUInteger ShadowListKitAsyncLate;
static double ShadowListKitAsyncLateWaitSum;
static double ShadowListKitAsyncLateWaitMax;

/*
 * Holds every bitmap back this long on the main queue, for the bench's self-test.
 */
static double ShadowListKitAsyncBenchDelay;

@implementation ShadowListKitTextView {
  NSUInteger _generation;
  CFTimeInterval _pendingSince;
  BOOL _pendingShown;
}

+ (NSDictionary *)shadowListKit_asyncStats
{
  return @{
    @"rendered" : @(ShadowListKitAsyncRendered),
    @"superseded" : @(ShadowListKitAsyncSuperseded),
    @"late" : @(ShadowListKitAsyncLate),
    @"lateWaitAvgMs" : @(ShadowListKitAsyncLate > 0 ? ShadowListKitAsyncLateWaitSum * 1000 / ShadowListKitAsyncLate : 0),
    @"lateWaitMaxMs" : @(ShadowListKitAsyncLateWaitMax * 1000),
  };
}

+ (void)shadowListKit_resetAsyncStats
{
  ShadowListKitAsyncRendered = 0;
  ShadowListKitAsyncSuperseded = 0;
  ShadowListKitAsyncLate = 0;
  ShadowListKitAsyncLateWaitSum = 0;
  ShadowListKitAsyncLateWaitMax = 0;
}

+ (void)shadowListKit_setBenchDrawDelay:(double)seconds
{
  ShadowListKitAsyncBenchDelay = MAX(0, seconds);
}

static void ShadowListKitTextViewsCountBlank(UIView *view, UIScrollView *scrollView, CGRect visible, NSUInteger *shown, NSUInteger *blank)
{
  for (UIView *child in view.subviews) {
    if (child.hidden || child.alpha < 0.01) {
      continue;
    }
    if ([child isKindOfClass:[ShadowListKitTextView class]]) {
      ShadowListKitTextView *text = (ShadowListKitTextView *)child;
      if (!text.displaysAsynchronously || text.textLayout.lineCount == 0 ||
          !CGRectIntersectsRect([text convertRect:text.bounds toView:scrollView], visible)) {
        continue;
      }
      ++*shown;
      if (text.layer.contents == nil) {
        ++*blank;
        if (text->_pendingSince > 0) {
          text->_pendingShown = YES;
        }
      }
      continue;
    }
    ShadowListKitTextViewsCountBlank(child, scrollView, visible, shown, blank);
  }
}

/*
 * The bench's probe: the share of asynchronously drawn text views in the scroll view's visible
 * area that show no bitmap yet.
 */
+ (double)shadowListKit_benchBlankFractionIn:(UIScrollView *)scrollView
{
  CGRect visible = UIEdgeInsetsInsetRect(scrollView.bounds, scrollView.adjustedContentInset);
  NSUInteger shown = 0;
  NSUInteger blank = 0;
  ShadowListKitTextViewsCountBlank(scrollView, scrollView, visible, &shown, &blank);
  return shown > 0 ? (double)blank / shown : 0;
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

- (BOOL)isOnScreen
{
  UIWindow *window = self.window;
  return window && !self.hidden && CGRectIntersectsRect([self convertRect:self.bounds toView:nil], window.bounds);
}

/*
 * Ends the request waiting for a bitmap, if any, and counts it when its view was seen blank.
 */
- (void)endPendingRender
{
  if (_pendingSince > 0 && _pendingShown) {
    double wait = CACurrentMediaTime() - _pendingSince;
    ++ShadowListKitAsyncLate;
    ShadowListKitAsyncLateWaitSum += wait;
    ShadowListKitAsyncLateWaitMax = MAX(ShadowListKitAsyncLateWaitMax, wait);
  }
  _pendingSince = 0;
  _pendingShown = NO;
}

- (void)redisplay
{
  ++_generation;
  if (!_displaysAsynchronously) {
    [self endPendingRender];
    [self setNeedsDisplay];
    return;
  }
  ShadowListKitTextLayout *layout = _textLayout;
  if (!layout || layout.lineCount == 0) {
    [self endPendingRender];
    self.layer.contents = nil;
    return;
  }
  if (_pendingSince > 0) {
    ++ShadowListKitAsyncSuperseded;
    [self endPendingRender];
  }
  NSUInteger generation = _generation;
  CGColorRef color = CGColorRetain([_textColor resolvedColorWithTraitCollection:self.traitCollection].CGColor);
  CGFloat scale = self.window.screen.scale ?: UIScreen.mainScreen.scale;
  CGSize size = layout.size;
  self.layer.contents = nil;
  _pendingSince = CACurrentMediaTime();
  double delay = ShadowListKitAsyncBenchDelay;
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
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      ShadowListKitTextView *view = weakSelf;
      if (view && view->_generation == generation) {
        view.layer.contentsScale = scale;
        view.layer.contents = (__bridge id)image.CGImage;
        ++ShadowListKitAsyncRendered;
        if ([view isOnScreen]) {
          view->_pendingShown = YES;
        }
        [view endPendingRender];
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
