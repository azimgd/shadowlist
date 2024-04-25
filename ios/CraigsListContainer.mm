#import "CraigsListContainer.h"

#import "CraigsListContainerComponentDescriptor.h"
#import "CraigsListContainerEventEmitter.h"
#import "CraigsListContainerProps.h"
#import "CraigsListContainerHelpers.h"

#import "RCTConversions.h"
#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface CraigsListContainer () <RCTCraigsListContainerViewProtocol>

@end

@implementation CraigsListContainer {
  UIScrollView* _scrollView;
  CraigsListContainerShadowNode::ConcreteState::Shared _state;
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
    _scrollView = [[UIScrollView alloc] init];
    _scrollView.delegate = self;
    _scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;

    self.contentView = _scrollView;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  assert(std::dynamic_pointer_cast<CraigsListContainerShadowNode::ConcreteState const>(state));
  _state = std::static_pointer_cast<CraigsListContainerShadowNode::ConcreteState const>(state);
  auto &data = _state->getData();

  auto scrollContent = RCTCGSizeFromSize(data.scrollContent);
  auto scrollContainer = RCTCGSizeFromSize(data.scrollContainer);

  self->_scrollView.contentSize = scrollContent;
  self->_scrollView.frame = CGRect{CGPointMake(0, 0), scrollContainer};
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  auto data = _state->getData();
  auto scrollPosition = RCTPointFromCGPoint(_scrollView.contentOffset);

  if (!CGPointEqualToPoint(_scrollView.contentOffset, RCTCGPointFromPoint(data.scrollPosition))) {
    data.scrollPosition = scrollPosition;
    _state->updateState(std::move(data));
  }
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self.contentView mountChildComponentView:childComponentView index:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self.contentView unmountChildComponentView:childComponentView index:index];
}


Class<RCTComponentViewProtocol> CraigsListContainerCls(void)
{
  return CraigsListContainer.class;
}

@end
