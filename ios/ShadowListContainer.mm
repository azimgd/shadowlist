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

  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  assert(std::dynamic_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(state));
  self->_state = std::static_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(state);
  const auto &stateData = _state->getData();
  const auto &props = static_cast<const ShadowListContainerProps &>(*_props);

  auto treeLengthHasChanged = false;
  if (oldState != nullptr) {
    auto oldStateData = std::static_pointer_cast<ShadowListContainerShadowNode::ConcreteState const>(oldState)->getData();
    treeLengthHasChanged = oldStateData.calculateTreeLength() == stateData.calculateTreeLength();
  }

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
  
  /*
   * Fill out empty content, make sure there are no state updates while this is filled out.
   */
  if (self->_childComponentViewPool.count && !self->_scrollContent.subviews.count) {
    auto extendedMetrics = stateData.calculateExtendedMetrics(RCTPointFromCGPoint(CGPointMake(0, 0)));
    [self updateChildrenIfNeeded:extendedMetrics.visibleStartIndex visibleEndIndex:extendedMetrics.visibleEndIndex];
  }
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  auto &stateData = _state->getData();
  auto extendedMetrics = stateData.calculateExtendedMetrics(RCTPointFromCGPoint(scrollView.contentOffset));

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
  auto headerIndex = 0;
  auto footerIndex = [self->_childComponentViewPool count] - 1;

  for (NSUInteger index = 0; index < visibleStartIndex; index++) {
    if (headerIndex == index) continue;
    if (footerIndex == index) continue;
    [self unmountChildComponentViewFromViewPool:index];
  }

  for (NSUInteger index = visibleEndIndex; index < [self->_childComponentViewPool count]; index++) {
    if (headerIndex == index) continue;
    if (footerIndex == index) continue;
    [self unmountChildComponentViewFromViewPool:index];
  }

  [self mountChildComponentViewFromViewPool:headerIndex];
  [self mountChildComponentViewFromViewPool:footerIndex];

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
  auto &stateData = _state->getData();
  auto nextOffset = CGPointMake(0, stateData.calculateItemOffset(index));
  
  [self->_scrollContainer setContentOffset:nextOffset animated:true];
  auto extendedMetrics = stateData.calculateExtendedMetrics(RCTPointFromCGPoint(nextOffset));
  [self updateChildrenIfNeeded:extendedMetrics.visibleStartIndex visibleEndIndex:extendedMetrics.visibleEndIndex];
}

Class<RCTComponentViewProtocol> ShadowListContainerCls(void)
{
  return ShadowListContainer.class;
}

@end
