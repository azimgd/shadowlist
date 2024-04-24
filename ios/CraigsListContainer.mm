#import "CraigsListContainer.h"

#import "CraigsListContainerComponentDescriptor.h"
#import "CraigsListContainerEventEmitter.h"
#import "CraigsListContainerProps.h"
#import "CraigsListContainerHelpers.h"

#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface CraigsListContainer () <RCTCraigsListContainerViewProtocol>

@end

@implementation CraigsListContainer {
    UIView * _view;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
    return concreteComponentDescriptorProvider<CraigsListContainerComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const CraigsListContainerProps>();
    _props = defaultProps;

    _view = [[UIView alloc] init];

    self.contentView = _view;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
    [super updateProps:props oldProps:oldProps];
}

Class<RCTComponentViewProtocol> CraigsListContainerCls(void)
{
    return CraigsListContainer.class;
}

@end
