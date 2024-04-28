#import "ShadowListItem.h"

#import "ShadowListItemComponentDescriptor.h"
#import "ShadowListItemEventEmitter.h"
#import "ShadowListItemProps.h"
#import "ShadowListItemHelpers.h"

#import "RCTConversions.h"
#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface ShadowListItem () <RCTShadowListItemViewProtocol>

@end

@implementation ShadowListItem {
  UIScrollView* _scrollView;
  ShadowListItemShadowNode::ConcreteState::Shared _state;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListItemComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListItemProps>();
    _props = defaultProps;
    _scrollView = [[UIScrollView alloc] init];
    _scrollView.delegate = self;

    self.contentView = _scrollView;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  [super updateProps:props oldProps:oldProps];
}

Class<RCTComponentViewProtocol> ShadowListItemCls(void)
{
  return ShadowListItem.class;
}

@end
