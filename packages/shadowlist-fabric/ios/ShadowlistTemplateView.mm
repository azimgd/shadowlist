#import "ShadowlistTemplateView.h"

#import "ShadowlistTemplateViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowlistViewSpec/Props.h>
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

@interface ShadowlistTemplateView () <RCTShadowlistTemplateViewViewProtocol>

@end

@implementation ShadowlistTemplateView {
  UIView * _view;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowlistTemplateViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowlistTemplateViewProps>();
    _props = defaultProps;

    _view = [[UIView alloc] init];

    self.contentView = _view;
  }

  return self;
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [_view insertSubview:childComponentView atIndex:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [childComponentView removeFromSuperview];
}

Class<RCTComponentViewProtocol> ShadowlistTemplateViewCls(void)
{
  return ShadowlistTemplateView.class;
}

@end
