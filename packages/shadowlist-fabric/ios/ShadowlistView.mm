#import "ShadowlistView.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowlistViewSpec/Props.h>
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"

#include <cmath>
#include <vector>

using namespace facebook::react;

/*
 * A scrollViewDidScroll offset within this many points of the offset the core
 * applied is its own echo, not a user scroll.
 */
static const CGFloat kScrollEchoTolerance = 2.0;

@interface ShadowlistView () <RCTShadowlistViewViewProtocol, UIScrollViewDelegate>

@end

@implementation ShadowlistView {
  ShadowlistViewShadowNode::ConcreteState::Shared _state;
  UIScrollView * _scrollView;
  UIView * _contentView;

  /*
   * Sticky header/footer are pinned natively here (not in the core layout) so they
   * track scrolling on the UI thread without the commit-cycle latency that made the
   * core-driven version choppy. The template views keep their resting position from
   * the shadow node; we layer a translation on top each scroll frame.
   */
  BOOL _stickyHeader;
  BOOL _stickyFooter;
  BOOL _horizontal;
  __weak UIView * _stickyHeaderView;
  __weak UIView * _stickyFooterView;

  /*
   * Sticky section headers (SectionList). Rather than pin a virtualized element
   * (which the JS layer would have to keep mounted, arriving several commits late
   * at each section boundary - the source of the scroll lag), the active section
   * header is rendered as an always-mounted overlay template (_sectionHeaderOverlay,
   * templateType "sectionHeader"). The core publishes the resting geometry (offset +
   * size along the scroll axis) of the in-flow section headers through state; we pin
   * the overlay to the viewport start every scroll tick (mirroring
   * Container::resolveStickyHeader), pushing it up as the next in-flow header
   * arrives. The overlay never unmounts, so its position never lags - only its
   * content swaps (in JS) at a boundary, masked by the real in-flow header behind it.
   */
  std::vector<int> _stickyHeaderIndices;
  std::vector<double> _stickyHeaderOffsets;
  std::vector<double> _stickyHeaderSizes;
  __weak UIView * _sectionHeaderOverlay;

  /*
   * The last offset the core applied itself. scrollViewDidScroll fires for both the
   * core's own setContentOffset and every user scroll, so we tell them apart by
   * comparing against this rather than isDragging/isDecelerating - those are only
   * set for touch gestures, so a trackpad / mouse-wheel scroll (e.g. the iOS
   * Simulator) would look programmatic and the core would never learn the user took
   * over, latching its inverted-pin / MVCP correction and blanking the list.
   */
  CGPoint _appliedOffset;
  BOOL _hasAppliedOffset;
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
    // A newly mounted element can land above a pinned header/footer or section
    // header in z-order and cover it; re-pin so the active sticky views stay on top
    // (and a freshly mounted active section header gets its translation at once).
    [self applyStickyTransforms];
    return;
  }

  if ([childComponentView conformsToProtocol:@protocol(RCTShadowlistTemplateViewViewProtocol)]) {
    const auto &templateProps = *std::static_pointer_cast<ShadowlistTemplateViewProps const>(childComponentView.props);
    if (templateProps.templateType == "footer") {
      _stickyFooterView = childComponentView;
    } else if (templateProps.templateType == "sectionHeader") {
      _sectionHeaderOverlay = childComponentView;
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
  if (childComponentView == _sectionHeaderOverlay) {
    _sectionHeaderOverlay = nil;
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
  _sectionHeaderOverlay = nil;
  _stickyHeaderIndices.clear();
  _stickyHeaderOffsets.clear();
  _stickyHeaderSizes.clear();
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

  [self applyStickySectionHeaders];
  [self bringStickyViewsToFront];
}

/*
 * Pin the always-mounted section-header overlay to the viewport start, mirroring
 * Container::resolveStickyHeader. The overlay rests at the content origin and shows
 * the active section's header (its content is swapped in JS); here we only move it.
 * Walk the core-published in-flow header geometry for the active section (the last
 * header resting at/above the scroll offset) and the next one, pin the overlay to
 * the viewport start, and push it up as the next in-flow header scrolls in. When no
 * section is active (scrolled above the first header) the overlay is hidden, so the
 * regular list header shows through.
 */
- (void)applyStickySectionHeaders
{
  if (!_sectionHeaderOverlay) {
    return;
  }

  if (_stickyHeaderIndices.empty()) {
    _sectionHeaderOverlay.hidden = YES;
    return;
  }

  CGFloat axisOffset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  if (axisOffset < 0.0) {
    axisOffset = 0.0;
  }

  /*
   * Headers are ascending by offset: the active one is the last resting at/above
   * the viewport start, and the first one past it is the "next" that pushes it up.
   */
  bool hasActive = false;
  double activeSize = 0.0;
  bool hasNext = false;
  double nextOffset = 0.0;
  for (size_t i = 0; i < _stickyHeaderOffsets.size(); i++) {
    double headerOffset = _stickyHeaderOffsets[i];
    if (headerOffset <= axisOffset) {
      hasActive = true;
      activeSize = _stickyHeaderSizes[i];
    } else {
      nextOffset = headerOffset;
      hasNext = true;
      break;
    }
  }

  if (!hasActive) {
    _sectionHeaderOverlay.hidden = YES;
    return;
  }

  /*
   * The overlay rests at the content origin, so its translation along the scroll
   * axis is simply its displayed top: pinned to the viewport start (axisOffset),
   * pushed up to nextOffset - activeSize as the next in-flow header arrives.
   */
  double translation = axisOffset;
  if (hasNext) {
    double pushedTop = nextOffset - activeSize;
    if (pushedTop < translation) {
      translation = pushedTop;
    }
  }

  _sectionHeaderOverlay.hidden = NO;
  _sectionHeaderOverlay.transform = _horizontal
    ? CGAffineTransformMakeTranslation(translation, 0.0)
    : CGAffineTransformMakeTranslation(0.0, translation);
  [_contentView bringSubviewToFront:_sectionHeaderOverlay];
}

/*
 * A pinned (sticky) header/footer must stay above the scrolling elements, which
 * mount continuously and would otherwise cover it.
 */
- (void)bringStickyViewsToFront
{
  if (_stickyHeader && _stickyHeaderView) {
    [_contentView bringSubviewToFront:_stickyHeaderView];
  }
  if (_stickyFooter && _stickyFooterView) {
    [_contentView bringSubviewToFront:_stickyFooterView];
  }
}

#pragma mark - State

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  _state = std::static_pointer_cast<ShadowlistViewShadowNode::ConcreteState const>(state);

  const auto &nextStateData = _state->getData();

  /*
   * Cache the sticky section-header geometry the core published this commit so the
   * per-scroll-tick pin (applyStickySectionHeaders) runs purely off cached values.
   */
  _stickyHeaderIndices.assign(nextStateData.stickyHeaderIndices_.begin(), nextStateData.stickyHeaderIndices_.end());
  _stickyHeaderOffsets.assign(nextStateData.stickyHeaderOffsets_.begin(), nextStateData.stickyHeaderOffsets_.end());
  _stickyHeaderSizes.assign(nextStateData.stickyHeaderSizes_.begin(), nextStateData.stickyHeaderSizes_.end());

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
    _appliedOffset = CGPointMake(
      nextStateData.containerOffsetX_,
      nextStateData.containerOffsetY_);
    _hasAppliedOffset = YES;
    _scrollView.contentOffset = _appliedOffset;
  }

  /*
   * Re-pin after the content size / offset changed so a sticky footer (whose pin
   * depends on contentSize) stays put even when the list is not actively scrolling.
   */
  [self applyStickyTransforms];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  if (!_state) {
    return;
  }

  /*
   * Distinguish a genuine user scroll from the offset the core applied itself by
   * comparing against the last applied offset (an "echo" of our own setContentOffset
   * lands within a pixel of it). This works for touch, trackpad and mouse-wheel
   * input alike, unlike isDragging/isDecelerating which only cover touch gestures.
   * Flagging the user scroll lets the core abandon an in-flight correction when the
   * user takes over (otherwise the correction latches and the visible window snaps
   * to a fixed anchor - a blank list on deep scroll).
   */
  CGPoint offset = scrollView.contentOffset;
  BOOL userScrolled = YES;
  if (_hasAppliedOffset &&
      fabs(offset.x - _appliedOffset.x) <= kScrollEchoTolerance &&
      fabs(offset.y - _appliedOffset.y) <= kScrollEchoTolerance) {
    userScrolled = NO;
    _hasAppliedOffset = NO;
  }

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

#pragma mark - Commands

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

- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
  /*
   * Direct offset scroll along the scroll axis. The core picks up the new position
   * from the resulting scroll callback, so no state round-trip is needed here.
   */
  CGPoint contentOffset = _horizontal
    ? CGPointMake(offset, _scrollView.contentOffset.y)
    : CGPointMake(_scrollView.contentOffset.x, offset);
  [_scrollView setContentOffset:contentOffset animated:animated];
}

- (void)scrollToEnd:(BOOL)animated
{
  if (!_state) {
    return;
  }

  /*
   * Core-driven: ride the scrollToIndex command channel with the SCROLL_TO_END_INDEX
   * sentinel (-3, see shadowlist-core/Constants.hpp) so the core converges on the
   * true bottom as off-screen rows are measured, instead of a one-shot jump to the
   * current contentSize - a stale, estimate-based bottom that stops short on a
   * variable-height list. The animated flag no longer applies (the core steps to the
   * bottom); the argument is kept for API compatibility.
   */
  (void)animated;
  auto nextStateData = _state->getData();
  nextStateData.containerOffsetIndex_ = -3.0;
  nextStateData.containerOffsetIndexNonce_ = nextStateData.containerOffsetIndexNonce_ + 1;
  nextStateData.containerOffsetEnabled_ = true;
  _state->updateState(std::move(nextStateData));
}

Class<RCTComponentViewProtocol> ShadowlistViewCls(void)
{
  return ShadowlistView.class;
}

@end
