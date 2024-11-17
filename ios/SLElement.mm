#import "SLElement.h"

#import "SLElementComponentDescriptor.h"
#import "SLElementEventEmitter.h"
#import "SLElementProps.h"
#import "SLElementHelpers.h"
#import "SLScrollable.h"

#import <React/RCTFabricComponentsPlugins.h>
#import <React/RCTConversions.h>

using namespace facebook::react;

@interface SLElement () <SLElementProtocol>

@end

@implementation SLElement {
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<SLElementComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const SLElementProps>();
    _props = defaultProps;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &prevViewProps = *std::static_pointer_cast<SLElementProps const>(_props);
  const auto &nextViewProps = *std::static_pointer_cast<SLElementProps const>(props);

  [super updateProps:props oldProps:oldProps];
}

Class<RCTComponentViewProtocol> SLElementCls(void)
{
  return SLElement.class;
}

@end
