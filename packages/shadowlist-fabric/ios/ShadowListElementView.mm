#import "ShadowListElementView.h"

#import "ShadowListElementViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowListViewSpec/Props.h>
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

@interface ShadowListElementView () <RCTShadowListElementViewViewProtocol>

@end

@implementation ShadowListElementView {
  RCTUIView *_contentView;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListElementViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListElementViewProps>();
    _props = defaultProps;

    _contentView = [[RCTUIView alloc] init];

    self.contentView = _contentView;
  }

  return self;
}

/*
 * Fabric shares recycled row views across every list in the app, even on other screens.
 * The base reset skips transform and hidden, and a drag sets both, so a row recycled mid drag
 * would show up shifted in the next list. Reset everything we set from native here.
 */
- (void)prepareForRecycle
{
  [self.layer removeAllAnimations];
  self.transform = CGAffineTransformIdentity;
  self.hidden = NO;
  self.layer.shadowOpacity = 0.0;
#if TARGET_OS_OSX
  // SLRaiseSubview raises this for sticky pinning. UIKit reorders subviews instead.
  self.layer.zPosition = 0.0;
#endif
  [super prepareForRecycle];
}

- (void)mountChildComponentView:(RCTUIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [_contentView insertSubview:childComponentView atIndex:index];
}

- (void)unmountChildComponentView:(RCTUIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [childComponentView removeFromSuperview];
}

Class<RCTComponentViewProtocol> ShadowListElementViewCls(void)
{
  return ShadowListElementView.class;
}

@end
