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
  if ([childComponentView conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
    const auto &childViewProps = *std::static_pointer_cast<ShadowlistElementViewProps const>(childComponentView.props);
    [_contentView insertSubview:childComponentView atIndex:childViewProps.index];
    return;
  }

  if ([childComponentView conformsToProtocol:@protocol(RCTShadowlistTemplateViewViewProtocol)]) {
    [_contentView addSubview:childComponentView];
    return;
  }
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [childComponentView removeFromSuperview];
}

#pragma mark - UIScrollViewDelegate

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  _state = std::static_pointer_cast<ShadowlistViewShadowNode::ConcreteState const>(state);

  const auto &nextStateData = _state->getData();

  _scrollView.contentSize = CGSizeMake(
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);
  _contentView.frame = CGRectMake(
    0,
    0,
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);

  SL_LOG("mm.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f)",
    nextStateData.totalContainerWidth_, nextStateData.totalContainerHeight_,
    nextStateData.containerOffsetEnabled_ ? 1 : 0,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    _scrollView.contentOffset.x, _scrollView.contentOffset.y);

  if (nextStateData.containerOffsetEnabled_) {
    _scrollView.contentOffset = CGPointMake(
      nextStateData.containerOffsetX_,
      nextStateData.containerOffsetY_);
  }
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  if (!_state) {
    return;
  }

  SL_LOG("mm.scrollViewDidScroll: offset=(%.1f,%.1f)", scrollView.contentOffset.x, scrollView.contentOffset.y);
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetX_ = scrollView.contentOffset.x;
  nextStateData.containerOffsetY_ = scrollView.contentOffset.y;
  nextStateData.containerOffsetEnabled_ = false;
  _state->updateState(std::move(nextStateData));
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowlistViewHandleCommand(self, commandName, args);
}

- (void)setStartReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  nextStateData.startReachedEnabled_ = enabled;
  _state->updateState(std::move(nextStateData));
}

- (void)setEndReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  nextStateData.endReachedEnabled_ = enabled;
  _state->updateState(std::move(nextStateData));
}

- (void)scrollToIndex:(NSInteger)index
{
  if (!_state) {
    return;
  }

  /*
   * Bump the nonce so the core treats this as a fresh request and re-scrolls even
   * when the index is unchanged from the previous call
   */
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetIndex_ = index;
  nextStateData.containerOffsetIndexNonce_ = nextStateData.containerOffsetIndexNonce_ + 1;
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

Class<RCTComponentViewProtocol> ShadowlistViewCls(void)
{
  return ShadowlistView.class;
}

@end
