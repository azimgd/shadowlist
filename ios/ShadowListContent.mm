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

- (CGPoint)listContainerScrollOffsetChange:(CGPoint)listContainerScrollOffset
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
  NSInteger visibleStartOffset;
  NSInteger visibleEndOffset;
  CGPoint visibleOffset;
  if (props.horizontal && props.inverted) {
    visibleStartOffset = contentViewTotal - listContainerScrollOffset.x - self->_contentView.frame.size.width;
    visibleEndOffset = contentViewTotal - listContainerScrollOffset.x;
    visibleStartIndex = self->_state->getData().contentViewMeasurements.lower_bound(visibleStartOffset);
    visibleEndIndex = self->_state->getData().contentViewMeasurements.lower_bound(visibleEndOffset);
  } else if (!props.horizontal && props.inverted) {
    visibleStartOffset = contentViewTotal - listContainerScrollOffset.y - self->_contentView.frame.size.height;
    visibleEndOffset = contentViewTotal - listContainerScrollOffset.y;
    visibleStartIndex = self->_state->getData().contentViewMeasurements.lower_bound(visibleStartOffset);
    visibleEndIndex = self->_state->getData().contentViewMeasurements.lower_bound(visibleEndOffset);
  } else if (props.horizontal && !props.inverted) {
    visibleStartOffset = listContainerScrollOffset.x;
    visibleEndOffset = listContainerScrollOffset.x + self.frame.size.width;
    visibleStartIndex = self->_state->getData().contentViewMeasurements.lower_bound(visibleStartOffset);
    visibleEndIndex = self->_state->getData().contentViewMeasurements.lower_bound(visibleEndOffset);
  } else {
    visibleStartOffset = listContainerScrollOffset.y;
    visibleEndOffset = listContainerScrollOffset.y + self.frame.size.height;
    visibleStartIndex = self->_state->getData().contentViewMeasurements.lower_bound(visibleStartOffset);
    visibleEndIndex = self->_state->getData().contentViewMeasurements.lower_bound(visibleEndOffset);
  }

  visibleStartIndex = MAX(0, visibleStartIndex - 2);
  visibleEndIndex = MIN(contentViewCount, visibleEndIndex + 2);

  [self->_cachedComponentPool recycle:visibleStartIndex visibleEndIndex:visibleEndIndex];
  
  if (props.horizontal) {
    return CGPointMake(visibleStartOffset, 0);
  } else {
    return CGPointMake(0, visibleStartOffset);
  }
}

- (CGPoint)listContainerScrollFocusIndexChange:(NSInteger)focusIndex
{
  assert(std::dynamic_pointer_cast<ShadowListContentShadowNode::ConcreteState const>(self->_state));
  const auto &stateData = self->_state->getData();
  const auto &props = static_cast<const ShadowListContentProps &>(*_props);

  const auto contentViewItem = stateData.contentViewMeasurements.sum((size_t)focusIndex);
  
  CGPoint listContainerScrollOffset;

  if (props.horizontal) {
    listContainerScrollOffset = CGPointMake(contentViewItem, 0);
  } else {
    listContainerScrollOffset = CGPointMake(0, contentViewItem);
  }

  [self listContainerScrollOffsetChange:listContainerScrollOffset];
  return listContainerScrollOffset;
}

- (CGPoint)listContainerScrollFocusOffsetChange:(NSInteger)focusOffset
{
  assert(std::dynamic_pointer_cast<ShadowListContentShadowNode::ConcreteState const>(self->_state));
  const auto &stateData = self->_state->getData();
  const auto &props = static_cast<const ShadowListContentProps &>(*_props);
  
  CGPoint listContainerScrollOffset;

  if (props.horizontal) {
    listContainerScrollOffset = CGPointMake(focusOffset, 0);
  } else {
    listContainerScrollOffset = CGPointMake(0, focusOffset);
  }

  [self listContainerScrollOffsetChange:listContainerScrollOffset];
  return listContainerScrollOffset;
}
Class<RCTComponentViewProtocol> ShadowListContentCls(void)
{
  return ShadowListContent.class;
}

@end
