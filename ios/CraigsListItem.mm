#import "CraigsListItem.h"

#import "CraigsListItemComponentDescriptor.h"
#import "CraigsListItemEventEmitter.h"
#import "CraigsListItemProps.h"
#import "CraigsListItemHelpers.h"

#import "RCTConversions.h"
#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface CraigsListItem () <RCTCraigsListItemViewProtocol>

@end

@implementation CraigsListItem {
  UIScrollView* _scrollView;
  CraigsListItemShadowNode::ConcreteState::Shared _state;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<CraigsListItemComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const CraigsListItemProps>();
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

Class<RCTComponentViewProtocol> CraigsListItemCls(void)
{
  return CraigsListItem.class;
}

@end
