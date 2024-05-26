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
  if ([self.delegate respondsToSelector:@selector(listContainerScrollChange:)]) {
    CGPoint listContainerScroll = scrollView.contentOffset;
    [self.delegate listContainerScrollChange:listContainerScroll];
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
  
  if ([self.delegate respondsToSelector:@selector(listContainerScrollChange:)]) {
    CGPoint listContainerScroll;
  
    /*
     * Scrollbar position adjustments
     */
    if (props.horizontal && props.inverted) {
      listContainerScroll = CGPointMake(self->_contentView.contentSize.width - self->_contentView.frame.size.width, 0);
      listContainerScroll.x = MAX(listContainerScroll.x, 0);
    } else if (!props.horizontal && props.inverted) {
      listContainerScroll = CGPointMake(0, self->_contentView.contentSize.height - self->_contentView.frame.size.height);
      listContainerScroll.y = MAX(listContainerScroll.y, 0);
    } else if (props.horizontal && !props.inverted) {
      listContainerScroll = CGPointZero;
    } else {
      listContainerScroll = CGPointZero;
    }

    /*
     * Manually trigger scrollevent for non-inverted list to run virtualization
     */
    if (CGPointEqualToPoint(listContainerScroll, CGPointZero)) {
      [self scrollViewDidScroll:self->_contentView];
    }

    [self->_contentView setContentOffset:listContainerScroll];
  }
}

- (void)listContentSizeChange:(CGSize)listContentSize {
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

#pragma mark - NativeCommands handlers

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowListContainerHandleCommand(self, commandName, args);
}

- (void)scrollToIndexNativeCommand:(int)index animated:(BOOL)animated
{
}

- (void)scrollToOffsetNativeCommand:(int)offset animated:(BOOL)animated
{
}

Class<RCTComponentViewProtocol> ShadowListContainerCls(void)
{
  return ShadowListContainer.class;
}

@end
