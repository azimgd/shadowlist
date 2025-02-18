#import "SLContent.h"

#import "SLContentComponentDescriptor.h"
#import "SLContentEventEmitter.h"
#import "SLContentProps.h"
#import "SLContentHelpers.h"

#import <React/RCTFabricComponentsPlugins.h>
#import <React/RCTConversions.h>

using namespace facebook::react;

@interface SLContent () <SLContentProtocol>

@end

@implementation SLContent {
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<SLContentComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const SLContentProps>();
    _props = defaultProps;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &prevViewProps = *std::static_pointer_cast<SLContentProps const>(_props);
  const auto &nextViewProps = *std::static_pointer_cast<SLContentProps const>(props);

  [super updateProps:props oldProps:oldProps];
}

Class<RCTComponentViewProtocol> SLContentCls(void)
{
  return SLContent.class;
}

@end
