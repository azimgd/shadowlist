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
  const auto &props = static_cast<const ShadowListContentProps &>(*_props);
  
  /*
   * Direction agnostic sum of all sizes, height for vertical, width for horizontal
   */
  const auto contentViewTotal = stateData.contentViewMeasurements.sum(stateData.contentViewMeasurements.size());
  
  if ([self.delegate respondsToSelector:@selector(listContentSizeChange:)]) {
    CGSize listContentSize;

    if (props.horizontal) {
      listContentSize = CGSizeMake(contentViewTotal, self->_contentView.frame.size.height);
    } else if (!props.horizontal) {
      listContentSize = CGSizeMake(self->_contentView.frame.size.width, contentViewTotal);
    }

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
  const auto &stateData = self->_state->getData();
  const auto &props = static_cast<const ShadowListContentProps &>(*_props);
  
  /*
   * Direction agnostic sum of all sizes, height for vertical, width for horizontal
   */
  const auto contentViewCount = stateData.contentViewMeasurements.size();
  const auto contentViewTotal = stateData.contentViewMeasurements.sum(contentViewCount);
  
  /*
   * Inverted scroll events on inverted scroll container
   */
  NSInteger visibleStartIndex;
  NSInteger visibleEndIndex;
  if (props.horizontal && props.inverted) {
    visibleStartIndex = self->_state->getData().contentViewMeasurements.lower_bound(
      contentViewTotal - listContainerScroll.x - self->_contentView.frame.size.width
    );
    visibleEndIndex = self->_state->getData().contentViewMeasurements.lower_bound(
      contentViewTotal - listContainerScroll.x
    );
  } else if (!props.horizontal && props.inverted) {
    visibleStartIndex = self->_state->getData().contentViewMeasurements.lower_bound(
      contentViewTotal - listContainerScroll.y - self->_contentView.frame.size.height
    );
    visibleEndIndex = self->_state->getData().contentViewMeasurements.lower_bound(
      contentViewTotal - listContainerScroll.y
    );
  } else if (props.horizontal && !props.inverted) {
    visibleStartIndex = self->_state->getData().contentViewMeasurements.lower_bound(
      listContainerScroll.x
    );
    visibleEndIndex = self->_state->getData().contentViewMeasurements.lower_bound(
      listContainerScroll.x + self.frame.size.width
    );
  } else {
    visibleStartIndex = self->_state->getData().contentViewMeasurements.lower_bound(
      listContainerScroll.y
    );
    visibleEndIndex = self->_state->getData().contentViewMeasurements.lower_bound(
      listContainerScroll.y + self.frame.size.height
    );
  }

  visibleStartIndex = MAX(0, visibleStartIndex - 2);
  visibleEndIndex = MIN(contentViewCount, visibleEndIndex + 2);

  [self->_cachedComponentPool recycle:visibleStartIndex visibleEndIndex:visibleEndIndex];
}

Class<RCTComponentViewProtocol> ShadowListContentCls(void)
{
  return ShadowListContent.class;
}

@end
