#import "ShadowlistView.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowlistViewSpec/Props.h>
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"

using namespace facebook::react;

@interface ShadowlistView () <RCTShadowlistViewViewProtocol, UIScrollViewDelegate>

@end

@implementation ShadowlistView {
  ShadowlistViewShadowNode::ConcreteState::Shared _state;
  UIScrollView * _scrollView;
  UIView * _contentView;

  /*
   * Sticky header/footer are pinned natively here (not in the core layout) so they
   * track scrolling on the UI thread without the commit-cycle latency that made the
   * core-driven version choppy. The core keeps the pin geometry (getSticky*Offset)
   * for a future core-driven integration. The template views keep their resting
   * frame from the shadow node; we layer a translation transform on top each frame.
   */
  BOOL _stickyHeader;
  BOOL _stickyFooter;
  BOOL _horizontal;
  __weak UIView * _stickyHeaderView;
  __weak UIView * _stickyFooterView;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<ShadowlistViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const ShadowlistViewProps>();
    _props = defaultProps;

    _scrollView = [[UIScrollView alloc] init];
    _scrollView.delegate = self;
    _scrollView.showsVerticalScrollIndicator = YES;
    _scrollView.showsHorizontalScrollIndicator = YES;
    _scrollView.scrollEnabled = YES;
    _scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    _scrollView.indicatorStyle = UIScrollViewIndicatorStyleWhite;

    _contentView = [[UIView alloc] init];
    [_scrollView addSubview:_contentView];

    self.contentView = _scrollView;
  }

  return self;
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if ([childComponentView conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
    const auto &childViewProps = *std::static_pointer_cast<ShadowlistElementViewProps const>(childComponentView.props);
    [_contentView insertSubview:childComponentView atIndex:childViewProps.index];
    return;
  }

  if ([childComponentView conformsToProtocol:@protocol(RCTShadowlistTemplateViewViewProtocol)]) {
    const auto &templateProps = *std::static_pointer_cast<ShadowlistTemplateViewProps const>(childComponentView.props);
    if (templateProps.templateType == "footer") {
      _stickyFooterView = childComponentView;
    } else {
      _stickyHeaderView = childComponentView;
    }
    [_contentView addSubview:childComponentView];
    [self applyStickyTransforms];
    return;
  }
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  if (childComponentView == _stickyHeaderView) {
    _stickyHeaderView = nil;
  }
  if (childComponentView == _stickyFooterView) {
    _stickyFooterView = nil;
  }
  [childComponentView removeFromSuperview];
}

- (void)prepareForRecycle
{
  _stickyHeaderView = nil;
  _stickyFooterView = nil;
  _stickyHeader = NO;
  _stickyFooter = NO;
  _horizontal = NO;
  [super prepareForRecycle];
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &nextProps = *std::static_pointer_cast<ShadowlistViewProps const>(props);
  _stickyHeader = nextProps.stickyHeader;
  _stickyFooter = nextProps.stickyFooter;
  _horizontal = nextProps.horizontal;

  [super updateProps:props oldProps:oldProps];

  [self applyStickyTransforms];
}

/*
 * Pin the sticky header/footer to the viewport by translating them relative to
 * their resting frame. The header tracks the scroll offset (stays at the start);
 * the footer tracks offset + window - content so it stays at the viewport end and
 * lands exactly on its resting position at the scroll extreme. Runs on every scroll
 * tick on the UI thread, so it stays in lockstep with the finger.
 */
- (void)applyStickyTransforms
{
  CGPoint offset = _scrollView.contentOffset;
  CGSize window = _scrollView.bounds.size;
  CGSize content = _scrollView.contentSize;

  if (_stickyHeaderView) {
    CGFloat translation = _stickyHeader ? (_horizontal ? offset.x : offset.y) : 0.0;
    _stickyHeaderView.transform = _horizontal
      ? CGAffineTransformMakeTranslation(translation, 0.0)
      : CGAffineTransformMakeTranslation(0.0, translation);
  }

  if (_stickyFooterView) {
    CGFloat translation = 0.0;
    if (_stickyFooter) {
      translation = _horizontal
        ? offset.x + window.width - content.width
        : offset.y + window.height - content.height;
    }
    _stickyFooterView.transform = _horizontal
      ? CGAffineTransformMakeTranslation(translation, 0.0)
      : CGAffineTransformMakeTranslation(0.0, translation);
  }
}

#pragma mark - UIScrollViewDelegate

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  _state = std::static_pointer_cast<ShadowlistViewShadowNode::ConcreteState const>(state);

  const auto &nextStateData = _state->getData();

  _scrollView.contentSize = CGSizeMake(
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);
  _contentView.frame = CGRectMake(
    0,
    0,
    nextStateData.totalContainerWidth_,
    nextStateData.totalContainerHeight_);

  SL_LOG("mm.updateState: contentSize=(%.1f,%.1f) enabled=%d offset=(%.1f,%.1f) curOffset=(%.1f,%.1f)",
    nextStateData.totalContainerWidth_, nextStateData.totalContainerHeight_,
    nextStateData.containerOffsetEnabled_ ? 1 : 0,
    nextStateData.containerOffsetX_, nextStateData.containerOffsetY_,
    _scrollView.contentOffset.x, _scrollView.contentOffset.y);

  if (nextStateData.containerOffsetEnabled_) {
    _scrollView.contentOffset = CGPointMake(
      nextStateData.containerOffsetX_,
      nextStateData.containerOffsetY_);
  }

  /*
   * Re-pin after the content size / offset changed so a sticky footer (whose pin
   * depends on contentSize) stays put even when the list is not actively scrolling.
   */
  [self applyStickyTransforms];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  if (!_state) {
    return;
  }

  /*
   * Distinguish a genuine user gesture from the offset the core applied itself: a
   * programmatic setContentOffset fires this with the scroll view neither dragging
   * nor decelerating. Flagging it lets the core abandon an in-flight correction
   * when the user takes over (otherwise a transient correction latches and freezes
   * the visible window - blank list on deep scroll).
   */
  BOOL userScrolled = scrollView.isDragging || scrollView.isDecelerating || scrollView.isTracking;

  SL_LOG("mm.scrollViewDidScroll: offset=(%.1f,%.1f) userScrolled=%d",
    scrollView.contentOffset.x, scrollView.contentOffset.y, userScrolled ? 1 : 0);
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetX_ = scrollView.contentOffset.x;
  nextStateData.containerOffsetY_ = scrollView.contentOffset.y;
  nextStateData.containerOffsetEnabled_ = false;
  nextStateData.userScrolled_ = userScrolled;
  _state->updateState(std::move(nextStateData));

  [self applyStickyTransforms];
}

/*
 * Clear the user-scroll flag once the gesture (and any momentum) ends, so a later
 * data/layout re-commit is not mistaken for a user scroll and does not cancel a
 * legitimate correction (e.g. a maintain-visible-content-position shift).
 */
- (void)clearUserScrolled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  if (!nextStateData.userScrolled_) {
    return;
  }
  nextStateData.userScrolled_ = false;
  _state->updateState(std::move(nextStateData));
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
  if (!decelerate) {
    [self clearUserScrolled];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
  [self clearUserScrolled];
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTShadowlistViewHandleCommand(self, commandName, args);
}

- (void)setStartReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  nextStateData.startReachedEnabled_ = enabled;
  _state->updateState(std::move(nextStateData));
}

- (void)setEndReachedEnabled:(BOOL)enabled
{
  if (!_state) {
    return;
  }

  auto nextStateData = _state->getData();
  nextStateData.endReachedEnabled_ = enabled;
  _state->updateState(std::move(nextStateData));
}

- (void)scrollToIndex:(NSInteger)index
{
  if (!_state) {
    return;
  }

  /*
   * Bump the nonce so the core treats this as a fresh request and re-scrolls even
   * when the index is unchanged from the previous call
   */
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetIndex_ = index;
  nextStateData.containerOffsetIndexNonce_ = nextStateData.containerOffsetIndexNonce_ + 1;
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

Class<RCTComponentViewProtocol> ShadowlistViewCls(void)
{
  return ShadowlistView.class;
}

@end
