#import "ShadowListContainer.h"
#import "ShadowListContent.h"

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
  UIScrollView* _contentView;
  ShadowListContainerShadowNode::ConcreteState::Shared _state;
  BOOL _scrollContainerScrolling;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowListContainerComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowListContainerProps>();
    _props = defaultProps;
    _contentView = [UIScrollView new];
    _contentView.delegate = self;
    _contentView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    self.contentView = _contentView;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  [super updateProps:props oldProps:oldProps];
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView {
  _scrollContainerScrolling = YES;
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate {
  if (!decelerate) {
    [self scrollViewDidEndScrolling:scrollView];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView {
  [self scrollViewDidEndScrolling:scrollView];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView {
  [self scrollViewDidEndScrolling:scrollView];
}

- (void)scrollViewDidEndScrolling:(UIScrollView *)scrollView {
  _scrollContainerScrolling = NO;
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
  if (_scrollContainerScrolling) {
    return self->_contentView;
  }
  return [super hitTest:point withEvent:event];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  const auto &eventEmitter = static_cast<const ShadowListContainerEventEmitter &>(*_eventEmitter);

  if ([self.delegate respondsToSelector:@selector(listContainerScrollOffsetUpdate:)]) {
    CGPoint listContainerScrollOffset = scrollView.contentOffset;
    [self.delegate listContainerScrollOffsetUpdate:listContainerScrollOffset];
  }
  
  int distanceFromEnd = [self measureDistanceFromEnd];
  int distanceFromStart = [self measureDistanceFromStart];

  if (distanceFromEnd > 0) {
    eventEmitter.onEndReached({ distanceFromEnd });
  }
  
  if (distanceFromStart > 0) {
    eventEmitter.onStartReached({ distanceFromStart });
  }
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_contentView mountChildComponentView:childComponentView index:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [self->_contentView unmountChildComponentView:childComponentView index:index];
}

- (void)layoutSubviews
{
  const auto &props = static_cast<const ShadowListContainerProps &>(*_props);

  RCTAssert(
    [self->_contentView.subviews.firstObject isKindOfClass:ShadowListContent.class],
    @"ShadowListContainer must be an ancestor of ShadowListContent");
  ShadowListContent *shadowListContent = self->_contentView.subviews.firstObject;
  shadowListContent.delegate = self;
  
  /*
   * Scrollbar position adjustments when initialScrollIndex not provided
   */
  if (props.initialScrollIndex && [self.delegate respondsToSelector:@selector(listContainerScrollFocusIndexUpdate:)]) {
    CGPoint listContainerScrollOffset = [self.delegate listContainerScrollFocusIndexUpdate:props.initialScrollIndex];
    [self->_contentView setContentOffset:listContainerScrollOffset];
  }

  /*
   * Scrollbar position adjustments when initialScrollIndex not provided
   */
  if (!props.initialScrollIndex && [self.delegate respondsToSelector:@selector(listContainerScrollFocusOffsetUpdate:)]) {
    NSInteger offset = 0;
  
    if (props.horizontal && props.inverted) {
      offset = MAX(self->_contentView.contentSize.width - self->_contentView.frame.size.width, 0);
    } else if (!props.horizontal && props.inverted) {
      offset = MAX(self->_contentView.contentSize.height - self->_contentView.frame.size.height, 0);
    }

    /*
     * Manually trigger scrollevent for non-inverted list to run virtualization
     */
    if (!offset) {
      [self scrollViewDidScroll:self->_contentView];
    }
    
    CGPoint listContainerScrollOffset = [self.delegate listContainerScrollFocusOffsetUpdate:offset];
    [self->_contentView setContentOffset:listContainerScrollOffset];
  }
}

- (void)listContentSizeUpdate:(CGSize)listContentSize {
  [self->_contentView setContentSize:listContentSize];

  /*
   * Stick scrollbar to bottom when scroll container is inverted vertically and to right when inverted horizontally
   */
  const auto &props = static_cast<const ShadowListContainerProps &>(*_props);
  if (props.horizontal && props.inverted) {
    CGPoint nextContentOffset = CGPointMake(self->_contentView.contentSize.height - self->_contentView.frame.size.height, 0);
    [self->_contentView setContentOffset:nextContentOffset];
  } else if (!props.horizontal && props.inverted) {
    CGPoint nextContentOffset = CGPointMake(0, self->_contentView.contentSize.height - self->_contentView.frame.size.height);
    [self->_contentView setContentOffset:nextContentOffset];
  }
}

- (void)visibleChildrenUpdate:(VisibleChildren)visibleChildren
{
  const auto &eventEmitter = static_cast<const ShadowListContainerEventEmitter &>(*_eventEmitter);
  eventEmitter.onVisibleChildrenUpdate({
    static_cast<int>(visibleChildren.visibleStartIndex),
    static_cast<int>(visibleChildren.visibleEndIndex),
    static_cast<int>(visibleChildren.visibleStartOffset),
    static_cast<int>(visibleChildren.visibleEndOffset),
    static_cast<int>(visibleChildren.headBlankStart),
    static_cast<int>(visibleChildren.headBlankEnd),
    static_cast<int>(visibleChildren.tailBlankStart),
    static_cast<int>(visibleChildren.tailBlankEnd)
  });
}

- (NSInteger)measureDistanceFromEnd {
  const auto &props = static_cast<const ShadowListContainerProps &>(*_props);

  if (props.horizontal && props.inverted) {
    auto triggerPoint = (props.onEndReachedThreshold * self->_contentView.frame.size.width);
    return self->_contentView.contentOffset.x >= triggerPoint ? self->_contentView.contentOffset.x : 0;
  } else if (!props.horizontal && props.inverted) {
    auto triggerPoint = (props.onEndReachedThreshold * self->_contentView.frame.size.height);
    return self->_contentView.contentOffset.y >= triggerPoint ? self->_contentView.contentOffset.y : 0;
  } else if (props.horizontal && !props.inverted) {
    auto triggerPoint = self->_contentView.contentSize.width - (props.onEndReachedThreshold * self->_contentView.frame.size.width);
    return self->_contentView.contentOffset.x >= triggerPoint ? self->_contentView.contentSize.width - self->_contentView.contentOffset.x : 0;
  } else {
    auto triggerPoint = self->_contentView.contentSize.height - (props.onEndReachedThreshold * self->_contentView.frame.size.height);
    return self->_contentView.contentOffset.y >= triggerPoint ? self->_contentView.contentSize.height - self->_contentView.contentOffset.y : 0;
  }
}

- (NSInteger)measureDistanceFromStart {
  const auto &props = static_cast<const ShadowListContainerProps &>(*_props);

  if (props.horizontal && props.inverted) {
    auto triggerPoint = self->_contentView.contentSize.width - (props.onStartReachedThreshold * self->_contentView.frame.size.width);
    return self->_contentView.contentOffset.x <= triggerPoint ? self->_contentView.contentSize.width - self->_contentView.contentOffset.x : 0;
  } else if (!props.horizontal && props.inverted) {
    auto triggerPoint = self->_contentView.contentSize.height - (props.onStartReachedThreshold * self->_contentView.frame.size.height);
    return self->_contentView.contentOffset.y <= triggerPoint ? self->_contentView.contentSize.height - self->_contentView.contentOffset.y : 0;
  } else if (props.horizontal && !props.inverted) {
    auto triggerPoint = (props.onStartReachedThreshold * self->_contentView.frame.size.width);
    return self->_contentView.contentOffset.x <= triggerPoint ? self->_contentView.contentOffset.x : 0;
  } else {
    auto triggerPoint = (props.onStartReachedThreshold * self->_contentView.frame.size.height);
    return self->_contentView.contentOffset.y <= triggerPoint ? self->_contentView.contentOffset.y : 0;
  }
}

#pragma mark - NativeCommands handlers

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowListContainerHandleCommand(self, commandName, args);
}

- (void)scrollToIndexNativeCommand:(int)index animated:(BOOL)animated
{
  if ([self.delegate respondsToSelector:@selector(listContainerScrollFocusIndexUpdate:)]) {
    CGPoint listContainerScrollOffset = [self.delegate listContainerScrollFocusIndexUpdate:(NSInteger)index];
    [self->_contentView setContentOffset:listContainerScrollOffset];
  }
}

- (void)scrollToOffsetNativeCommand:(int)offset animated:(BOOL)animated
{
  if ([self.delegate respondsToSelector:@selector(listContainerScrollFocusOffsetUpdate:)]) {
    CGPoint listContainerScrollOffset = [self.delegate listContainerScrollFocusOffsetUpdate:offset];
    [self->_contentView setContentOffset:listContainerScrollOffset];
  }
}

Class<RCTComponentViewProtocol> ShadowListContainerCls(void)
{
  return ShadowListContainer.class;
}

@end
