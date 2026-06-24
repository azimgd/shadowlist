#import "ShadowListTemplateView.h"

#import "ShadowListTemplateViewComponentDescriptor.h"
#import <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowListViewSpec/Props.h>
#import <react/renderer/components/ShadowListViewSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

@interface ShadowListTemplateView () <RCTShadowListTemplateViewViewProtocol>

@end

@implementation ShadowListTemplateView {
  RCTUIView * _contentView;
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

    _contentView = [[RCTUIView alloc] init];

    self.contentView = _contentView;
  }

  return self;
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
