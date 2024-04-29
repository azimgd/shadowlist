#import "ShadowListContainer.h"

#import "ShadowListContainerComponentDescriptor.h"
#import "ShadowListContainerEventEmitter.h"
#import "ShadowListContainerProps.h"
#import "ShadowListContainerHelpers.h"

#import "RCTConversions.h"
#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface ShadowListContainer () <RCTShadowListContainerViewProtocol>

@end

@implementation ShadowListContainer {
  UIScrollView* _scrollContainer;
  UIView* _scrollContent;
  bool _scrollInverted;
  ShadowListContainerShadowNode::ConcreteState::Shared _state;
  NSMutableArray<UIView<RCTComponentViewProtocol> *> *_childComponentViewPool;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListContainerComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListContainerProps>();
    
    _childComponentViewPool = [NSMutableArray array];
    
    _props = defaultProps;
    _scrollInverted = defaultProps->inverted;
    _scrollContent = [UIView new];
    _scrollContainer = [UIScrollView new];
    _scrollContainer.delegate = self;
    _scrollContainer.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;

    [_scrollContainer addSubview:_scrollContent];
    self.contentView = _scrollContainer;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &oldConcreteProps = static_cast<const ShadowListContainerProps &>(*_props);
  const auto &newConcreteProps = static_cast<const ShadowListContainerProps &>(*props);

  if (newConcreteProps.inverted != oldConcreteProps.inverted) {
  }

  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  assert(std::dynamic_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(state));
  self->_state = std::static_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(state);
  auto &data = _state->getData();

  auto scrollContent = RCTCGSizeFromSize(data.scrollContent);
  auto scrollContainer = RCTCGSizeFromSize(data.scrollContainer);

  self->_scrollContainer.contentSize = scrollContent;
  self->_scrollContainer.frame = CGRect{CGPointMake(0, 0), scrollContainer};
  
  /*
   * Fill out empty content, make sure there are no state updates while this is filled out.
   */
  if (self->_childComponentViewPool.count && !self->_scrollContent.subviews.count) {
    auto extendedMetrics = data.calculateExtendedMetrics(RCTPointFromCGPoint(CGPointMake(0, 0)));
    [self updateChildrenIfNeeded:extendedMetrics.visibleStartIndex visibleEndIndex:extendedMetrics.visibleEndIndex];
  }
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  auto &data = _state->getData();
  auto extendedMetrics = data.calculateExtendedMetrics(RCTPointFromCGPoint(scrollView.contentOffset));

  [self updateChildrenIfNeeded:extendedMetrics.visibleStartIndex visibleEndIndex:extendedMetrics.visibleEndIndex];
}

/*
 * Mount component into subview
 */
- (void)mountChildComponentViewFromViewPool:(NSInteger)index {
  if ([self->_childComponentViewPool count] <= index) {
    return;
  }
  UIView<RCTComponentViewProtocol> *childComponentView = self->_childComponentViewPool[index];
  [self->_scrollContent insertSubview:childComponentView atIndex:index];
}

/*
 * Mount component from superview
 */
- (void)unmountChildComponentViewFromViewPool:(NSInteger)index {
  if ([self->_scrollContent.subviews count] <= index) {
    return;
  }
  [[self->_scrollContent.subviews objectAtIndex:index] removeFromSuperview];
}

/*
 * Unmount children that are out of visible area, and mount that are in
 */
- (void)updateChildrenIfNeeded:(int)visibleStartIndex visibleEndIndex:(int)visibleEndIndex
{
  for (NSUInteger index = 0; index < visibleStartIndex; index++) {
    [self unmountChildComponentViewFromViewPool:index];
  }
  for (NSUInteger index = visibleEndIndex; index < [self->_childComponentViewPool count]; index++) {
    [self unmountChildComponentViewFromViewPool:index];
  }

  for (NSUInteger index = visibleStartIndex; index < visibleEndIndex; index++) {
    [self mountChildComponentViewFromViewPool:index];
  }
  
  static_cast<const ShadowListContainerEventEmitter&>(*_eventEmitter).onVisibleChange({visibleStartIndex, visibleEndIndex});
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_childComponentViewPool insertObject:childComponentView atIndex:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_childComponentViewPool removeObjectAtIndex:index];
  [self unmountChildComponentViewFromViewPool:index];
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowListContainerHandleCommand(self, commandName, args);
}

- (void)scrollToIndex:(int)index
{
  auto &data = _state->getData();
  auto nextOffset = CGPointMake(0, data.calculateItemOffset(index));
  
  [self->_scrollContainer setContentOffset:nextOffset animated:true];
  auto extendedMetrics = data.calculateExtendedMetrics(RCTPointFromCGPoint(nextOffset));
  [self updateChildrenIfNeeded:extendedMetrics.visibleStartIndex visibleEndIndex:extendedMetrics.visibleEndIndex];
}

Class<RCTComponentViewProtocol> ShadowListContainerCls(void)
{
  return ShadowListContainer.class;
}

@end
