#import "ShadowlistElementView.h"

#import "ShadowlistElementViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowlistViewSpec/Props.h>
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

@interface ShadowlistElementView () <RCTShadowlistElementViewViewProtocol>

@end

@implementation ShadowlistElementView {
  UIView * _view;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowlistElementViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowlistElementViewProps>();
    _props = defaultProps;

    _view = [[UIView alloc] init];

    self.contentView = _view;
  }

  return self;
}

Class<RCTComponentViewProtocol> ShadowlistElementViewCls(void)
{
  return ShadowlistElementView.class;
}

@end
