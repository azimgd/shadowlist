#import "ShadowlistView.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowlistViewSpec/Props.h>
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface ShadowlistView () <RCTShadowlistViewViewProtocol, UIScrollViewDelegate>

@end

@implementation ShadowlistView {
  ShadowlistViewShadowNode::ConcreteState::Shared _state;
  UIScrollView * _scrollView;
  UIView * _contentView;
  
  /*
   * Scroll Events → State (suspends sending scroll position to state)
   * Scroll events don't update state when set to true
   */
  bool _suspenseMvcp;

  /*
   * State → Scroll Position (allows receiving scroll position from state)
   * State can update scroll position when set to true
   */
  bool _suspenseScroll;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowlistViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowlistViewProps>();
    _props = defaultProps;

    _suspenseScroll = true;

    _scrollView = [[UIScrollView alloc] init];
    _scrollView.delegate = self;
    _scrollView.showsVerticalScrollIndicator = YES;
    _scrollView.showsHorizontalScrollIndicator = YES;
    _scrollView.scrollEnabled = YES;
    _scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    _scrollView.indicatorStyle = UIScrollViewIndicatorStyleWhite;

    _contentView = [[UIView alloc] init];
    [_scrollView addSubview:_contentView];

    self.contentView = _scrollView;
  }

  return self;
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if (![childComponentView conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
    return;
  }

  const auto &childViewProps = *std::static_pointer_cast<ShadowlistElementViewProps const>(childComponentView.props);

  [_contentView insertSubview:childComponentView atIndex:childViewProps.index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [childComponentView removeFromSuperview];
}

#pragma mark - UIScrollViewDelegate

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  self->_state = std::static_pointer_cast<ShadowlistViewShadowNode::ConcreteState const>(state);

  const auto &nextStateData = self->_state->getData();

  self->_scrollView.contentSize = CGSizeMake(
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);
  self->_contentView.frame = CGRectMake(
    0,
    0,
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);

  if (self->_suspenseScroll) {
    self->_scrollView.contentOffset = CGPointMake(
      nextStateData.containerOffsetX_,
      nextStateData.containerOffsetY_);
  }
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  self->_suspenseScroll = false;

  if (self->_suspenseMvcp) {
    return;
  }

  auto nextStateData = self->_state->getData();
  nextStateData.containerOffsetX_ = scrollView.contentOffset.x;
  nextStateData.containerOffsetY_ = scrollView.contentOffset.y;
  _state->updateState(std::move(nextStateData));
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowlistViewHandleCommand(self, commandName, args);
}

- (void)prependElements:(NSInteger)size
{
  self->_suspenseMvcp = true;
  self->_suspenseScroll = true;

  // suspense state updates temporarily (for 1frame) until mvcp adjustments are completed
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(16 * NSEC_PER_MSEC));
  dispatch_after(timeout, dispatch_get_main_queue(), ^(void){
    self->_suspenseMvcp = false;
  });
}

- (void)appendElements:(NSInteger)size
{
  self->_suspenseMvcp = true;
  self->_suspenseScroll = true;
  
  // suspense state updates temporarily (for 1frame) until mvcp adjustments are completed
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(16 * NSEC_PER_MSEC));
  dispatch_after(timeout, dispatch_get_main_queue(), ^(void){
    self->_suspenseMvcp = false;
  });
}

Class<RCTComponentViewProtocol> ShadowlistViewCls(void)
{
  return ShadowlistView.class;
}

@end
