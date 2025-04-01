#import "SLContainer.h"

#import "SLContainerComponentDescriptor.h"
#import "SLContainerEventEmitter.h"
#import "SLContainerProps.h"
#import "SLContainerHelpers.h"
#import "SLElementProps.h"

#import <React/RCTFabricComponentsPlugins.h>
#import <React/RCTConversions.h>

using namespace azimgd::shadowlist;
using namespace facebook::react;

@interface SLContainer () <SLContainerProtocol>

@end

@implementation SLContainer {
  UIScrollView *_scrollContent;
  SLContainerShadowNode::ConcreteState::Shared _state;
  bool scrollContentCompleted;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<SLContainerComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const SLContainerProps>();
    _props = defaultProps;
    _scrollContent = [UIScrollView new];
    _scrollContent.delegate = self;
    _scrollContent.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    
    self.contentView = _scrollContent;
  }

  return self;
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_scrollContent mountChildComponentView:childComponentView index:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_scrollContent unmountChildComponentView:childComponentView index:index];
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &prevViewProps = *std::static_pointer_cast<SLContainerProps const>(self->_props);
  const auto &nextViewProps = *std::static_pointer_cast<SLContainerProps const>(props);

  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  self->_state = std::static_pointer_cast<SLContainerShadowNode::ConcreteState const>(state);

  const auto &nextStateData = self->_state->getData();
  
  if (nextStateData.scrollContentUpdated) {
    CGSize scrollContent = RCTCGSizeFromSize(nextStateData.scrollContent);
    [self->_scrollContent setContentSize:scrollContent];
    [self->_scrollContent.subviews.firstObject setFrame:CGRectMake(0, 0, scrollContent.width, scrollContent.height)];
  }

  if (nextStateData.scrollPositionUpdated) {
    CGPoint scrollPosition = RCTCGPointFromPoint(nextStateData.scrollPosition);
    [self->_scrollContent setContentOffset:scrollPosition];
  }
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  if (
    (scrollView.contentOffset.x < 0) ||
    (scrollView.contentOffset.y < 0)
  ) {
    return;
  }

  auto scrollPosition = RCTPointFromCGPoint(scrollView.contentOffset);
  _state->updateState([scrollPosition](const SLContainerShadowNode::ConcreteState::Data &data) {
    auto newData = data;
    newData.scrollPosition = scrollPosition;
    newData.scrollContentCompleted = false;
    return std::make_shared<const SLContainerShadowNode::ConcreteState::Data>(newData);
  });
}

- (void)scrollContentCompleted:(UIScrollView *)scrollView
{
  if (!self->scrollContentCompleted) {
    return;
  }
  
  auto scrollPosition = RCTPointFromCGPoint(scrollView.contentOffset);
  _state->updateState([scrollPosition](const SLContainerShadowNode::ConcreteState::Data &data) {
    auto newData = data;
    newData.scrollPosition = scrollPosition;
    newData.scrollContentCompleted = true;
    return std::make_shared<const SLContainerShadowNode::ConcreteState::Data>(newData);
  });
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  self->scrollContentCompleted = NO;
  [self scrollContentCompleted:scrollView];
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
  if(!decelerate) {
    self->scrollContentCompleted = YES;
    [self scrollContentCompleted:scrollView];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
  self->scrollContentCompleted = YES;
  [self scrollContentCompleted:scrollView];
}

- (void)scrollViewDidScrollToTop:(UIScrollView *)scrollView
{
  self->scrollContentCompleted = YES;
  [self scrollContentCompleted:scrollView];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
  self->scrollContentCompleted = YES;
  [self scrollContentCompleted:scrollView];
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  SLContainerHandleCommand(self, commandName, args);
}

- (void)scrollToIndexNativeCommand:(int)index animated:(BOOL)animated
{
}

- (void)scrollToOffsetNativeCommand:(int)offset animated:(BOOL)animated
{
}

- (void)prepareForRecycle
{
  [super prepareForRecycle];

  const auto data = std::static_pointer_cast<SLContainerShadowNode::ConcreteState const>(_state);
  const auto &nextStateData = self->_state->getData();

  nextStateData.registryManager.lock()->cleanup();
}

Class<RCTComponentViewProtocol> SLContainerCls(void)
{
  return SLContainer.class;
}

@end
