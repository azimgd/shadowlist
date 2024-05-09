#import "ShadowListContainer.h"

#import "ShadowListContainerComponentDescriptor.h"
#import "ShadowListContainerEventEmitter.h"
#import "ShadowListContainerProps.h"
#import "ShadowListContainerHelpers.h"
#import "CachedComponentPool/CachedComponentPool.h"

#import "RCTConversions.h"
#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface ShadowListContainer () <RCTShadowListContainerViewProtocol>

@end

@implementation ShadowListContainer {
  UIScrollView* _scrollContainer;
  UIView* _scrollContent;
  ShadowListContainerShadowNode::ConcreteState::Shared _state;
  CachedComponentPool *_cachedComponentPool;
  BOOL _cachedComponentPoolChanged;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListContainerComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListContainerProps>();

    _cachedComponentPoolChanged = true;
    
    _props = defaultProps;
    _scrollContent = [UIView new];
    _scrollContainer = [UIScrollView new];
    _scrollContainer.delegate = self;
    _scrollContainer.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;

    [_scrollContainer addSubview:_scrollContent];
    self.contentView = _scrollContainer;
    
    _cachedComponentPool = [[CachedComponentPool alloc] initWithObservable:^(NSInteger poolIndex) {
      [self->_scrollContent addSubview:[self->_cachedComponentPool getComponentView:poolIndex]];
    } onCachedComponentUnmount:^(NSInteger poolIndex) {
      [[self->_cachedComponentPool getComponentView:poolIndex] removeFromSuperview];
    }];
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &oldConcreteProps = static_cast<const ShadowListContainerProps &>(*_props);
  const auto &newConcreteProps = static_cast<const ShadowListContainerProps &>(*props);

  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  assert(std::dynamic_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(state));
  self->_state = std::static_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(state);
  const auto &stateData = _state->getData();
  const auto &props = static_cast<const ShadowListContainerProps &>(*_props);

  auto scrollContent = RCTCGSizeFromSize(stateData.scrollContent);
  auto scrollContainer = RCTCGSizeFromSize(stateData.scrollContainer);

  self->_scrollContainer.contentSize = scrollContent;
  self->_scrollContainer.frame = CGRect{CGPointMake(0, 0), scrollContainer};

  if (props.initialScrollIndex) {
    auto nextInitialScrollIndex = props.initialScrollIndex + (props.hasListHeaderComponent ? 1 : 0);
    [self->_scrollContainer setContentOffset:CGPointMake(0, stateData.calculateItemOffset(nextInitialScrollIndex)) animated:false];
  } else if (props.inverted) {
    [self->_scrollContainer setContentOffset:CGPointMake(0, stateData.scrollContent.height - stateData.scrollContainer.height) animated:false];
  }
  
  [self recycle];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  [self recycle];
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_cachedComponentPool upsertCachedComponentPoolItem:childComponentView poolIndex:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_cachedComponentPool removeCachedComponentPoolItem:childComponentView poolIndex:index];
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowListContainerHandleCommand(self, commandName, args);
}

- (void)scrollToIndex:(int)index
{
  auto &stateData = _state->getData();
  auto nextOffset = CGPointMake(0, stateData.calculateItemOffset(index));
  [self->_scrollContainer setContentOffset:nextOffset animated:true];
  
  [self recycle];
}

- (void)recycle {
  assert(std::dynamic_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(_state));
  auto &stateData = _state->getData();
  auto extendedMetrics = stateData.calculateExtendedMetrics(RCTPointFromCGPoint(self->_scrollContainer.contentOffset));
  [self->_cachedComponentPool recycle:extendedMetrics.visibleStartIndex visibleEndIndex:extendedMetrics.visibleEndIndex];
}

Class<RCTComponentViewProtocol> ShadowListContainerCls(void)
{
  return ShadowListContainer.class;
}

@end
