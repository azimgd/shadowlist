#import "ShadowListContent.h"
#import "ShadowListContainer.h"

#import "ShadowListContentComponentDescriptor.h"
#import "ShadowListContentEventEmitter.h"
#import "ShadowListContentProps.h"
#import "ShadowListContentHelpers.h"
#import "CachedComponentPool/CachedComponentPool.h"

#import "RCTConversions.h"
#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface ShadowListContent () <RCTShadowListContentViewProtocol>

@end

@implementation ShadowListContent {
  UIView* _contentView;
  ShadowListContentShadowNode::ConcreteState::Shared _state;
  CachedComponentPool *_cachedComponentPool;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListContentComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListContentProps>();
    _props = defaultProps;
    _contentView = [UIView new];
    self.contentView = _contentView;
    
    auto onCachedComponentMount = ^(NSInteger poolIndex) {
      [self->_contentView insertSubview:[self->_cachedComponentPool getComponentView:poolIndex] atIndex:poolIndex];
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

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView superview]) [childComponentView removeFromSuperview];
  [self->_cachedComponentPool insertCachedComponentPoolItem:childComponentView poolIndex:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView superview]) [childComponentView removeFromSuperview];
  [self->_cachedComponentPool removeCachedComponentPoolItem:childComponentView poolIndex:index];
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  assert(std::dynamic_pointer_cast<ShadowListContentShadowNode::ConcreteState const>(state));
  self->_state = std::static_pointer_cast<ShadowListContentShadowNode::ConcreteState const>(state);
  const auto &stateData = _state->getData();
  
  if ([self.delegate respondsToSelector:@selector(listContentSizeChange:)]) {
    CGSize listContentSize = CGSizeMake(
      self->_contentView.frame.size.width,
      stateData.contentViewMeasurements.sum(stateData.contentViewMeasurements.size())
    );
    [self.delegate listContentSizeChange:listContentSize];
  }
}

- (void)layoutSubviews
{
  RCTAssert(
    [self.superview.superview isKindOfClass:ShadowListContainer.class],
    @"ShadowListContent must be a descendant of ShadowListContainer");
  ShadowListContainer *shadowListContainer = (ShadowListContainer *)self.superview.superview;
  shadowListContainer.delegate = self;
}

- (void)listContainerScrollChange:(CGPoint)listContainerScroll
{
  assert(std::dynamic_pointer_cast<ShadowListContentShadowNode::ConcreteState const>(self->_state));
  size_t visibleStartIndex = self->_state->getData().contentViewMeasurements.lower_bound(listContainerScroll.y);
  size_t visibleEndIndex = self->_state->getData().contentViewMeasurements.lower_bound(listContainerScroll.y + self.frame.size.height);
  [self->_cachedComponentPool recycle:visibleStartIndex visibleEndIndex:visibleEndIndex];
}

Class<RCTComponentViewProtocol> ShadowListContentCls(void)
{
  return ShadowListContent.class;
}

@end
