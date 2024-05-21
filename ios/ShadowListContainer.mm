#import "ShadowListContainer.h"

#import "ShadowListContainerComponentDescriptor.h"
#import "ShadowListContainerEventEmitter.h"
#import "ShadowListContainerProps.h"
#import "ShadowListContainerHelpers.h"
#import "Scrollable.h"
#import "CachedComponentPool/CachedComponentPool.h"

#import "RCTConversions.h"
#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface ShadowListContainer () <RCTShadowListContainerViewProtocol>

@end

@implementation ShadowListContainer {
  UIScrollView* _scrollContainer;
  ShadowListContainerShadowNode::ConcreteState::Shared _state;
  CachedComponentPool *_cachedComponentPool;
  int _cachedComponentPoolDriftCount;
  BOOL _scrollContainerLayoutComplete;
  BOOL _scrollContainerLayoutHorizontal;
  BOOL _scrollContainerLayoutInverted;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListContainerComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListContainerProps>();

    _cachedComponentPoolDriftCount = 0;
    _scrollContainerLayoutComplete = false;
    _scrollContainerLayoutInverted = defaultProps->inverted;
    _scrollContainerLayoutHorizontal = defaultProps->horizontal;
    
    _props = defaultProps;
    _scrollContainer = [UIScrollView new];
    _scrollContainer.delegate = self;
    _scrollContainer.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;

    self.contentView = _scrollContainer;
    
    auto onCachedComponentMount = ^(NSInteger poolIndex) {
      [self->_scrollContainer addSubview:[self->_cachedComponentPool getComponentView:poolIndex]];
    };
    auto onCachedComponentUnmount = ^(NSInteger poolIndex) {
      [[self->_cachedComponentPool getComponentView:poolIndex] removeFromSuperview];
    };
    _cachedComponentPool = [[CachedComponentPool alloc] initWithObservable:@[]
      onCachedComponentMount:onCachedComponentMount
      onCachedComponentUnmount:onCachedComponentUnmount];
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &oldConcreteProps = static_cast<const ShadowListContainerProps &>(*_props);
  const auto &newConcreteProps = static_cast<const ShadowListContainerProps &>(*props);

  self->_scrollContainerLayoutInverted = newConcreteProps.inverted;
  self->_scrollContainerLayoutHorizontal = newConcreteProps.horizontal;

  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  assert(std::dynamic_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(state));
  self->_state = std::static_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(state);
  const auto &stateData = _state->getData();
  const auto &props = static_cast<const ShadowListContainerProps &>(*_props);

  self->_scrollContainer.contentSize = RCTCGSizeFromSize(stateData.scrollContent);
  self->_scrollContainer.frame.size = RCTCGSizeFromSize(stateData.scrollContainer);

  if (!self->_scrollContainerLayoutComplete && props.initialScrollIndex) {
    auto nextInitialScrollIndex = props.initialScrollIndex + (props.hasListHeaderComponent ? 1 : 0);
    [self scrollRespectfully:stateData.calculateItemOffset(nextInitialScrollIndex) animated:false];
  } else if (!self->_scrollContainerLayoutComplete && props.inverted) {
    auto scrollContainerSize = Scrollable::getScrollContentSize(stateData.scrollContainer, self->_scrollContainerLayoutHorizontal);
    auto scrollContentSize = Scrollable::getScrollContentSize(stateData.scrollContent, self->_scrollContainerLayoutHorizontal);
    [self scrollRespectfully:(scrollContentSize - scrollContainerSize) animated:false];
  }

  if (!self->_scrollContainerLayoutComplete) {
    self->_scrollContainerLayoutComplete = true;
  }

  _cachedComponentPoolDriftCount = stateData.countTree() - [self->_cachedComponentPool countPool];

  [self recycle];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  const auto &props = static_cast<const ShadowListContainerProps &>(*_props);
  const auto &eventEmitter = static_cast<const ShadowListContainerEventEmitter &>(*_eventEmitter);
  
  auto distanceFromEnd = [self distanceFromEndRespectfully:props.onEndReachedThreshold];
  if (distanceFromEnd > 0) {
    eventEmitter.onEndReached({ distanceFromEnd = distanceFromEnd });
  }
  
  [self recycle];
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView superview]) [childComponentView removeFromSuperview];
  [self->_cachedComponentPool insertCachedComponentPoolItem:childComponentView poolIndex:index];
  
  if (_cachedComponentPoolDriftCount > 0) {
    _cachedComponentPoolDriftCount -= 1;
    if (_cachedComponentPoolDriftCount == 0) [self recycle];
  }
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView superview]) [childComponentView removeFromSuperview];
  [self->_cachedComponentPool removeCachedComponentPoolItem:childComponentView poolIndex:index];
}

- (int)distanceFromEndRespectfully:(float)threshold {
  if (self->_scrollContainerLayoutHorizontal) {
    auto containerSize = self->_scrollContainer.bounds.size.width;
    auto contentSize = self->_scrollContainer.contentSize.width;
    auto offset = self->_scrollContainer.contentOffset.x;
    
    auto triggerPoint = contentSize - (threshold * containerSize);
    return offset >= triggerPoint ? (int)(contentSize - offset) : 0;
  } else {
    auto containerSize = self->_scrollContainer.bounds.size.height;
    auto contentSize = self->_scrollContainer.contentSize.height;
    auto offset = self->_scrollContainer.contentOffset.y;
    
    auto triggerPoint = contentSize - (threshold * containerSize);
    return offset >= triggerPoint ? (int)(contentSize - offset) : 0;
  }
}

- (void)scrollRespectfully:(float)contentOffset animated:(BOOL)animated
{
  if (self->_scrollContainerLayoutInverted) {
    [self->_scrollContainer setContentOffset:CGPointMake(contentOffset, 0) animated:animated];
  } else {
    [self->_scrollContainer setContentOffset:CGPointMake(0, contentOffset) animated:animated];
  }
}

#pragma mark - NativeCommands handlers

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowListContainerHandleCommand(self, commandName, args);
}

- (void)scrollToIndexNativeCommand:(int)index animated:(BOOL)animated
{
  auto &stateData = _state->getData();
  [self scrollRespectfully:stateData.calculateItemOffset(index) animated:animated];

  [self recycle];
}

- (void)scrollToOffsetNativeCommand:(int)offset animated:(BOOL)animated
{
  [self scrollRespectfully:offset animated:animated];

  [self recycle];
}

- (void)recycle {
  assert(std::dynamic_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(_state));
  auto &stateData = _state->getData();
  auto extendedMetrics = stateData.calculateExtendedMetrics(
    RCTPointFromCGPoint(self->_scrollContainer.contentOffset),
    self->_scrollContainerLayoutHorizontal,
    self->_scrollContainerLayoutInverted
  );
  [self->_cachedComponentPool recycle:extendedMetrics.visibleStartIndex visibleEndIndex:extendedMetrics.visibleEndIndex];
}

Class<RCTComponentViewProtocol> ShadowListContainerCls(void)
{
  return ShadowListContainer.class;
}

@end
