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
 * The lines of layout drawn into a bitmap of its size at scale, top down. Thread safe.
 */
static CGImageRef ShadowListKitTextCreateImage(ShadowListKitTextLayout *layout, CGColorRef color, CGFloat scale)
{
  CGSize size = layout.size;
  size_t width = (size_t)ceil(size.width * scale);
  size_t height = (size_t)ceil(size.height * scale);
  if (width == 0 || height == 0) {
    return NULL;
  }
  CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  CGContextRef context = CGBitmapContextCreate(NULL, width, height, 8, 0, space,
    (CGBitmapInfo)kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
  CGColorSpaceRelease(space);
  if (!context) {
    return NULL;
  }
  CGContextTranslateCTM(context, 0, (CGFloat)height);
  CGContextScaleCTM(context, scale, -scale);
  [layout drawAtPoint:CGPointZero inContext:context color:color];
  CGImageRef image = CGBitmapContextCreateImage(context);
  CGContextRelease(context);
  return image;
}

@implementation ShadowListKitTextView {
  NSUInteger _generation;
}

- (instancetype)initWithFrame:(NSRect)frame
{
  if (self = [super initWithFrame:frame]) {
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
    self.layer.contentsGravity = kCAGravityTopLeft;
    _textColor = NSColor.labelColor;
  }
  return self;
}

- (BOOL)isFlipped
{
  return YES;
}

- (BOOL)wantsUpdateLayer
{
  return YES;
}

- (nullable NSView *)hitTest:(NSPoint)point
{
  return nil;
}

- (void)setTextLayout:(ShadowListKitTextLayout *)textLayout
{
  if (_textLayout == textLayout) {
    return;
  }
  _textLayout = textLayout;
  [self redisplay];
}

- (void)setTextColor:(NSColor *)textColor
{
  if ([_textColor isEqual:textColor]) {
    return;
  }
  _textColor = textColor;
  [self redisplay];
}

- (void)setDisplaysAsynchronously:(BOOL)displaysAsynchronously
{
  if (_displaysAsynchronously == displaysAsynchronously) {
    return;
  }
  _displaysAsynchronously = displaysAsynchronously;
  [self redisplay];
}

- (void)viewDidChangeEffectiveAppearance
{
  [super viewDidChangeEffectiveAppearance];
  [self redisplay];
}

- (void)viewDidChangeBackingProperties
{
  [super viewDidChangeBackingProperties];
  [self redisplay];
}

- (CGFloat)backingScale
{
  return self.window.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor ?: 1;
}

/*
 * The text color as the view's appearance shows it.
 */
- (CGColorRef)resolvedTextColor CF_RETURNS_RETAINED
{
  __block CGColorRef color = NULL;
  [self.effectiveAppearance performAsCurrentDrawingAppearance:^{
    color = CGColorRetain(self->_textColor.CGColor);
  }];
  return color;
}

- (void)redisplay
{
  ++_generation;
  if (!_displaysAsynchronously) {
    self.needsDisplay = YES;
    return;
  }
  ShadowListKitTextLayout *layout = _textLayout;
  self.layer.contents = nil;
  if (!layout || layout.lineCount == 0) {
    return;
  }
  NSUInteger generation = _generation;
  CGColorRef color = [self resolvedTextColor];
  CGFloat scale = [self backingScale];
  __weak ShadowListKitTextView *weakSelf = self;
  dispatch_async(ShadowListKitTextRenderQueue(), ^{
    CGImageRef image = ShadowListKitTextCreateImage(layout, color, scale);
    CGColorRelease(color);
    dispatch_async(dispatch_get_main_queue(), ^{
      ShadowListKitTextView *view = weakSelf;
      if (view && view->_generation == generation) {
        view.layer.contentsScale = scale;
        view.layer.contents = (__bridge id)image;
      }
      CGImageRelease(image);
    });
  });
}

- (void)updateLayer
{
  if (_displaysAsynchronously) {
    return;
  }
  if (!_textLayout || _textLayout.lineCount == 0) {
    self.layer.contents = nil;
    return;
  }
  CGColorRef color = [self resolvedTextColor];
  CGFloat scale = [self backingScale];
  CGImageRef image = ShadowListKitTextCreateImage(_textLayout, color, scale);
  CGColorRelease(color);
  self.layer.contentsScale = scale;
  self.layer.contents = (__bridge id)image;
  CGImageRelease(image);
}

@end
