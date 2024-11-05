#import "SLContainer.h"

#import "SLContainerComponentDescriptor.h"
#import "SLContainerEventEmitter.h"
#import "SLContainerProps.h"
#import "SLContainerHelpers.h"
#import "SLContainerChildrenManager.h"

#import <React/RCTFabricComponentsPlugins.h>
#import <React/RCTConversions.h>

using namespace facebook::react;

@interface SLContainer () <SLContainerProtocol>

@end

@implementation SLContainer {
  UIScrollView * _contentView;
  SLContainerShadowNode::ConcreteState::Shared _state;
  SLContainerChildrenManager *_containerChildrenManager;
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
    _contentView = [UIScrollView new];
    _contentView.delegate = self;
    _contentView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    _containerChildrenManager = [[SLContainerChildrenManager alloc] initWithContentView:_contentView];
    
    self.contentView = _contentView;
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
  const auto &prevViewProps = *std::static_pointer_cast<SLContainerProps const>(_props);
  const auto &nextViewProps = *std::static_pointer_cast<SLContainerProps const>(props);

  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  self->_state = std::static_pointer_cast<SLContainerShadowNode::ConcreteState const>(state);
  const auto &nextStateData = _state->getData();
  
  int visibleStartIndex = nextStateData.visibleStartIndex;
  int visibleEndIndex = nextStateData.visibleEndIndex == 0 ?
    nextStateData.initialNumToRender :
    nextStateData.visibleEndIndex;

  [self->_containerChildrenManager mount:visibleStartIndex end:visibleEndIndex];
  [self->_contentView setContentSize:RCTCGSizeFromSize(nextStateData.scrollContent)];
  [self->_contentView setContentOffset:RCTCGPointFromPoint(nextStateData.scrollPosition)];
  
  const auto &eventEmitter = static_cast<const SLContainerEventEmitter &>(*_eventEmitter);
  eventEmitter.onVisibleChange({visibleStartIndex, visibleEndIndex});
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  auto stateData = _state->getData();
  stateData.scrollPosition = RCTPointFromCGPoint(scrollView.contentOffset);
  stateData.visibleStartIndex = stateData.calculateVisibleStartIndex(
    stateData.getScrollPosition(RCTPointFromCGPoint(scrollView.contentOffset))
  );
  stateData.visibleEndIndex = stateData.calculateVisibleEndIndex(
    stateData.getScrollPosition(RCTPointFromCGPoint(scrollView.contentOffset))
  );
  self->_state->updateState(std::move(stateData));
}

Class<RCTComponentViewProtocol> SLContainerCls(void)
{
  return SLContainer.class;
}

@end
