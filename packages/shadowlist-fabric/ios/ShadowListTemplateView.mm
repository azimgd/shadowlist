#import "ShadowListTemplateView.h"

#import "ShadowListTemplateViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowListViewSpec/Props.h>
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

@interface ShadowListTemplateView () <RCTShadowListTemplateViewViewProtocol>

@end

@implementation ShadowListTemplateView {
  RCTUIView *_contentView;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListTemplateViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListTemplateViewProps>();
    _props = defaultProps;

    _contentView = [RCTUIView new];

    self.contentView = _contentView;
  }

  return self;
}

/*
 * Fabric shares recycled template views across every list in the app, even on other screens.
 * The base reset skips transform and hidden, and sticky pinning sets both.
 * A hidden section overlay recycled as another list's header would never show again,
 * yet it still takes up space. Reset both so a recycled view starts fresh.
 */
- (void)prepareForRecycle
{
  self.transform = CGAffineTransformIdentity;
  self.hidden = NO;
#if TARGET_OS_OSX
  // SLRaiseSubview raises this to keep sticky views above the rows. UIKit reorders instead.
  self.layer.zPosition = 0.0;
#endif
  [super prepareForRecycle];
}

/*
 * Children sit where Yoga put them, padding included. The base view would also inset the
 * content view by the padding, which applied it twice.
 */
- (void)updateLayoutMetrics:(const LayoutMetrics &)layoutMetrics oldLayoutMetrics:(const LayoutMetrics &)oldLayoutMetrics
{
  [super updateLayoutMetrics:layoutMetrics oldLayoutMetrics:oldLayoutMetrics];
  _contentView.frame = CGRectMake(0, 0, layoutMetrics.frame.size.width, layoutMetrics.frame.size.height);
}

- (void)mountChildComponentView:(RCTUIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [_contentView insertSubview:childComponentView atIndex:index];
}

- (void)unmountChildComponentView:(RCTUIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [childComponentView removeFromSuperview];
}

Class<RCTComponentViewProtocol> ShadowListTemplateViewCls(void)
{
  return ShadowListTemplateView.class;
}

@end
