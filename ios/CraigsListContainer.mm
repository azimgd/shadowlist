#import "CraigsListContainer.h"

#import "CraigsListContainerComponentDescriptor.h"
#import "CraigsListContainerEventEmitter.h"
#import "CraigsListContainerProps.h"
#import "CraigsListContainerHelpers.h"

#import "RCTConversions.h"
#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface CraigsListContainer () <RCTCraigsListContainerViewProtocol>

@end

@implementation CraigsListContainer {
  UIScrollView* _scrollContainer;
  UIView* _scrollContent;
  CraigsListContainerShadowNode::ConcreteState::Shared _state;
  NSMutableArray<UIView<RCTComponentViewProtocol> *> *_childComponentViewPool;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<CraigsListContainerComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const CraigsListContainerProps>();
    
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
  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  assert(std::dynamic_pointer_cast<CraigsListContainerShadowNode::ConcreteState const>(state));
  self->_state = std::static_pointer_cast<CraigsListContainerShadowNode::ConcreteState const>(state);
  auto &data = _state->getData();

  auto scrollContent = RCTCGSizeFromSize(data.scrollContent);
  auto scrollContainer = RCTCGSizeFromSize(data.scrollContainer);

  self->_scrollContainer.contentSize = scrollContent;
  self->_scrollContainer.frame = CGRect{CGPointMake(0, 0), scrollContainer};
  
  /**
   * Fill out empty content, make sure there are no state updates while this is filled out.
   */
  if (self->_childComponentViewPool.count && !self->_scrollContent.subviews.count) {
    auto layoutMetrics = data.calculateLayoutMetrics(RCTPointFromCGPoint(CGPointMake(0, 0)));
    [self updateChildrenIfNeeded:layoutMetrics.visibleStartIndex visibleEndIndex:layoutMetrics.visibleEndIndex];
  }
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  auto &data = _state->getData();
  auto layoutMetrics = data.calculateLayoutMetrics(RCTPointFromCGPoint(scrollView.contentOffset));

  [self updateChildrenIfNeeded:layoutMetrics.visibleStartIndex visibleEndIndex:layoutMetrics.visibleEndIndex];
}

- (void)updateChildrenIfNeeded:(int)visibleStartIndex visibleEndIndex:(int)visibleEndIndex
{
  for (UIView *childComponentView in self->_scrollContent.subviews) {
    [childComponentView removeFromSuperview];
  }

  for (NSUInteger index = visibleStartIndex; index < visibleEndIndex; index++) {
    UIView<RCTComponentViewProtocol> *childComponentView = self->_childComponentViewPool[index];
    [self->_scrollContent insertSubview:childComponentView atIndex:index];
  }
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_childComponentViewPool insertObject:childComponentView atIndex:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_childComponentViewPool removeObjectAtIndex:index];
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTCraigsListContainerHandleCommand(self, commandName, args);
}

- (void)scrollToIndex:(int)index
{
  auto &data = _state->getData();
  auto nextOffset = CGPointMake(0, data.calculateItemOffset(index));
  
  [self->_scrollContainer setContentOffset:nextOffset animated:true];
  auto layoutMetrics = data.calculateLayoutMetrics(RCTPointFromCGPoint(nextOffset));
  [self updateChildrenIfNeeded:layoutMetrics.visibleStartIndex visibleEndIndex:layoutMetrics.visibleEndIndex];
}

Class<RCTComponentViewProtocol> CraigsListContainerCls(void)
{
  return CraigsListContainer.class;
}

@end
