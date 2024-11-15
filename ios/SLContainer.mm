#import "SLContainer.h"

#import "SLContainerComponentDescriptor.h"
#import "SLContainerEventEmitter.h"
#import "SLContainerProps.h"
#import "SLContainerHelpers.h"
#import "SLContainerChildrenManager.h"
#import "SLScrollable.h"

#import <React/RCTFabricComponentsPlugins.h>
#import <React/RCTConversions.h>

using namespace facebook::react;

@interface SLContainer () <SLContainerProtocol>

@end

@implementation SLContainer {
  UIScrollView *_scrollContent;
  UIRefreshControl *_scrollContentRefresh;
  SLContainerShadowNode::ConcreteState::Shared _state;
  SLContainerChildrenManager *_containerChildrenManager;
  SLScrollable *_scrollable;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<SLContainerComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const SLContainerProps>();
    _props = defaultProps;
    _scrollContent = [UIScrollView new];
    _scrollContent.delegate = self;
    _scrollContent.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    _containerChildrenManager = [[SLContainerChildrenManager alloc] initWithContentView:_scrollContent];
    _scrollable = [SLScrollable new];
    
    _scrollContentRefresh = [UIRefreshControl new];
    [_scrollContentRefresh addTarget:self action:@selector(handleRefresh) forControlEvents:UIControlEventValueChanged];
    [_scrollContent addSubview:_scrollContentRefresh];
    
    self.contentView = _scrollContent;
  }

  return self;
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_containerChildrenManager mountChildComponentView:childComponentView index:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_containerChildrenManager unmountChildComponentView:childComponentView index:index];
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &prevViewProps = *std::static_pointer_cast<SLContainerProps const>(self->_props);
  const auto &nextViewProps = *std::static_pointer_cast<SLContainerProps const>(props);

  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  self->_state = std::static_pointer_cast<SLContainerShadowNode::ConcreteState const>(state);
  const auto &nextStateData = _state->getData();
  const auto &nextViewProps = *std::static_pointer_cast<SLContainerProps const>(self->_props);

  int visibleStartIndex = nextStateData.visibleStartIndex;
  int visibleEndIndex = nextStateData.visibleEndIndex;

  [self->_containerChildrenManager mount:visibleStartIndex end:visibleEndIndex];
  [self->_scrollContent setContentSize:RCTCGSizeFromSize(nextStateData.scrollContent)];
  [self->_scrollContent setContentOffset:RCTCGPointFromPoint(nextStateData.scrollPosition)];

  [self->_scrollable updateState:nextStateData.horizontal
    inverted:nextViewProps.inverted
    visibleStartTrigger:nextStateData.calculateVisibleStartTrigger(
      nextStateData.getScrollPosition(nextStateData.scrollPosition)
    )
    visibleEndTrigger:nextStateData.calculateVisibleEndTrigger(
      nextStateData.getScrollPosition(nextStateData.scrollPosition)
    )
    scrollContainerWidth:nextStateData.scrollContainer.width
    scrollContainerHeight:nextStateData.scrollContainer.height
    scrollContentWidth:nextStateData.scrollContent.width
    scrollContentHeight:nextStateData.scrollContent.height];

  const auto &eventEmitter = static_cast<const SLContainerEventEmitter &>(*_eventEmitter);
  eventEmitter.onVisibleChange({visibleStartIndex, visibleEndIndex});
  
  int distanceFromEnd = [self->_scrollable shouldNotify:RCTCGPointFromPoint(nextStateData.scrollPosition)];
  if (distanceFromEnd) {
    eventEmitter.onEndReached({distanceFromEnd});
  }
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  /**
   * Disable optimization when nearing the end of the list to ensure scrollPosition remains in sync.
   * Required to prevent content shifts when adding items to the list.
   */
  if (
    [self->_scrollable shouldNotify:scrollView.contentOffset] == 0 &&
    ![self->_scrollable shouldUpdate:scrollView.contentOffset]) {
    return;
  }

  auto stateData = _state->getData();
  int visibleStartIndex = stateData.calculateVisibleStartIndex(
    stateData.getScrollPosition(stateData.scrollPosition)
  );
  int visibleEndIndex = stateData.calculateVisibleEndIndex(
    stateData.getScrollPosition(stateData.scrollPosition)
  );
  stateData.scrollPosition = RCTPointFromCGPoint(scrollView.contentOffset);
  stateData.visibleStartIndex = visibleStartIndex;
  stateData.visibleEndIndex = visibleEndIndex;
  
  self->_state->updateState(std::move(stateData));
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  SLContainerHandleCommand(self, commandName, args);
}

- (void)scrollToIndexNativeCommand:(int)index animated:(BOOL)animated
{
  auto stateData = _state->getData();
  stateData.scrollPosition = stateData.calculateScrollPositionOffset(stateData.childrenMeasurements.sum(index));
  stateData.visibleStartIndex = stateData.calculateVisibleStartIndex(
    stateData.getScrollPosition(stateData.scrollPosition)
  );
  stateData.visibleEndIndex = stateData.calculateVisibleEndIndex(
    stateData.getScrollPosition(stateData.scrollPosition)
  );
  self->_state->updateState(std::move(stateData));
}

- (void)scrollToOffsetNativeCommand:(int)offset animated:(BOOL)animated
{
  auto stateData = _state->getData();
  stateData.scrollPosition = stateData.calculateScrollPositionOffset(offset);
  stateData.visibleStartIndex = stateData.calculateVisibleStartIndex(
    stateData.getScrollPosition(stateData.scrollPosition)
  );
  stateData.visibleEndIndex = stateData.calculateVisibleEndIndex(
    stateData.getScrollPosition(stateData.scrollPosition)
  );
  self->_state->updateState(std::move(stateData));
}

- (void)handleRefresh {
}

Class<RCTComponentViewProtocol> SLContainerCls(void)
{
  return SLContainer.class;
}

@end
