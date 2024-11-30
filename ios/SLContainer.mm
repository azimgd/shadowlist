#import "SLContainer.h"

#import "SLContainerComponentDescriptor.h"
#import "SLContainerEventEmitter.h"
#import "SLContainerProps.h"
#import "SLContainerHelpers.h"
#import "SLContainerChildrenManager.h"
#import "SLScrollable.h"
#import "SLElementProps.h"
#import "helpers.h"

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
  const auto &nextViewProps = *std::static_pointer_cast<SLElementProps const>([childComponentView props]);
  const auto uniqueId = [NSString stringWithUTF8String:nextViewProps.uniqueId.c_str()];
  [self->_containerChildrenManager mountChildComponentView:childComponentView uniqueId:uniqueId index:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  const auto &nextViewProps = *std::static_pointer_cast<SLElementProps const>([childComponentView props]);
  const auto uniqueId = [NSString stringWithUTF8String:nextViewProps.uniqueId.c_str()];
  [self->_containerChildrenManager unmountChildComponentView:childComponentView uniqueId:uniqueId index:index];
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

  const auto &nextStateData = self->_state->getData();
  const auto &nextViewProps = *std::static_pointer_cast<SLContainerProps const>(self->_props);

  [self->_scrollable updateState:nextStateData.horizontal
    inverted:nextViewProps.inverted
    scrollContainerWidth:nextStateData.scrollContainer.width
    scrollContainerHeight:nextStateData.scrollContainer.height
    scrollContentWidth:nextStateData.scrollContent.width
    scrollContentHeight:nextStateData.scrollContent.height];

  CGPoint scrollPositionCGPoint = CGPointMake(
    nextStateData.scrollPosition.x + self->_scrollContent.contentOffset.x,
    nextStateData.scrollPosition.y + self->_scrollContent.contentOffset.y
  );
  facebook::react::Point scrollPositionPoint = RCTPointFromCGPoint(scrollPositionCGPoint);

  CGSize scrollContent = RCTCGSizeFromSize(nextStateData.scrollContent);
  int visibleStartIndex = adjustVisibleStartIndex(
    nextStateData.childrenMeasurementsTree.lower_bound([self->_scrollable getVisibleStartOffset:scrollPositionCGPoint]),
    nextStateData.childrenMeasurementsTree.size()
  );
  int visibleEndIndex = adjustVisibleEndIndex(
    nextStateData.childrenMeasurementsTree.lower_bound([self->_scrollable getVisibleEndOffset:scrollPositionCGPoint]),
    nextStateData.childrenMeasurementsTree.size()
  );

  [self->_containerChildrenManager mount:visibleStartIndex visibleEndIndex:visibleEndIndex];

  [self->_scrollContent setContentSize:scrollContent];
  [self->_scrollContent setContentOffset:scrollPositionCGPoint];

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(16 * NSEC_PER_MSEC)), dispatch_get_main_queue(), ^{
    [self updateVirtualization];
  });
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  [self updateVirtualization];
}

- (void)updateVirtualization
{
  if (self->_state == nullptr) return;

  const auto &nextStateData = self->_state->getData();
  const auto &nextViewProps = *std::static_pointer_cast<SLContainerProps const>(self->_props);

  CGPoint scrollPositionCGPoint = self->_scrollContent.contentOffset;
  facebook::react::Point scrollPositionPoint = RCTPointFromCGPoint(scrollPositionCGPoint);

  int visibleStartIndex = adjustVisibleStartIndex(
    nextStateData.childrenMeasurementsTree.lower_bound([self->_scrollable getVisibleStartOffset:scrollPositionCGPoint]),
    nextStateData.childrenMeasurementsTree.size()
  );
  int visibleEndIndex = adjustVisibleEndIndex(
    nextStateData.childrenMeasurementsTree.lower_bound([self->_scrollable getVisibleEndOffset:scrollPositionCGPoint]),
    nextStateData.childrenMeasurementsTree.size()
  );
  int visibleStartOffset = nextStateData.childrenMeasurementsTree.sum(visibleStartIndex);
  int visibleEndOffset = (nextViewProps.horizontal ?
      self->_scrollContent.contentSize.width :
      self->_scrollContent.contentSize.height) - nextStateData.childrenMeasurementsTree.sum(visibleEndIndex);

  [self->_containerChildrenManager
    mount:visibleStartIndex
    visibleEndIndex:visibleEndIndex];

  [self updateObservers:scrollPositionCGPoint
    visibleStartIndex:visibleStartIndex
    visibleEndIndex:visibleEndIndex
    visibleStartOffset:visibleStartOffset
    visibleEndOffset:visibleEndOffset];
}

- (void)updateObservers:(CGPoint)scrollPosition visibleStartIndex:(int)visibleStartIndex visibleEndIndex:(int)visibleEndIndex visibleStartOffset:(float)visibleStartOffset visibleEndOffset:(float)visibleEndOffset
{
  if (_eventEmitter == nullptr) {
    return;
  }

  /**
   * Dispatch event emitters
   */
  const auto &eventEmitter = static_cast<const SLContainerEventEmitter &>(*_eventEmitter);
  
  if ([self->_scrollable shouldNotifyChange]) {
    eventEmitter.onVisibleChange({visibleStartIndex, visibleEndIndex, visibleStartOffset, visibleEndOffset});
  }
  
  int distanceFromStart = [self->_scrollable shouldNotifyStart:scrollPosition];
  if (distanceFromStart) {
    eventEmitter.onStartReached({distanceFromStart});
  }

  int distanceFromEnd = [self->_scrollable shouldNotifyEnd:scrollPosition];
  if (distanceFromEnd) {
    eventEmitter.onEndReached({distanceFromEnd});
  }
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  SLContainerHandleCommand(self, commandName, args);
}

- (void)scrollToIndexNativeCommand:(int)index animated:(BOOL)animated
{
  auto headerFooter = 1;
  auto nextStateData = self->_state->getData();
  auto offset = adjustVisibleStartIndex(
    nextStateData.childrenMeasurementsTree.sum(index + headerFooter),
    nextStateData.childrenMeasurementsTree.size()
  );

  [self->_scrollContent setContentOffset:[self->_scrollable getScrollPositionFromOffset:offset] animated:animated];
}

- (void)scrollToOffsetNativeCommand:(int)offset animated:(BOOL)animated
{
  [self->_scrollContent setContentOffset:[self->_scrollable getScrollPositionFromOffset:offset] animated:animated];
}

- (void)handleRefresh {
}

Class<RCTComponentViewProtocol> SLContainerCls(void)
{
  return SLContainer.class;
}

@end
