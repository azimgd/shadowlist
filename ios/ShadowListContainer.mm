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
  const auto &props = static_cast<const ShadowListContainerProps &>(*_props);
  const auto &eventEmitter = static_cast<const ShadowListContainerEventEmitter &>(*_eventEmitter);
  
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

-(void)layoutSubviews
{
  ShadowListContent *shadowListContent = [self->_contentView.subviews firstObject];
  shadowListContent.delegate = self;
}

- (void)listContentSizeChange:(CGSize)listContentSize {
  [self->_contentView setContentSize:listContentSize];
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
