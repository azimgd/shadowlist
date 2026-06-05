#import "ShadowlistView.h"
#import "ShadowlistView+Internal.h"

#import "ShadowlistViewComponentDescriptor.h"
#import <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#import <react/renderer/components/ShadowlistViewSpec/Props.h>
#import <react/renderer/components/ShadowlistViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"

#include <cmath>

using namespace facebook::react;

/*
 * A scrollViewDidScroll offset within this many points of the offset the core
 * applied is its own echo, not a user scroll.
 */
static const CGFloat kScrollEchoTolerance = 2.0;

/*
 * The host list view: lifecycle, child mounting, state/props application, the scroll
 * delegate and the imperative commands. Sticky pinning lives in ShadowlistView+Sticky
 * and drag-to-reorder in ShadowlistView+DragReorder; both reach the shared ivars
 * through ShadowlistView+Internal.h.
 */
@implementation ShadowlistView

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

    /*
     * Long press to pick a row up. It lives on the scroll view so a quick swipe still
     * scrolls (the press has to settle for minimumPressDuration first); once it fires
     * we disable scrolling and drive the offset ourselves. Disabled until the
     * dragEnabled prop turns it on.
     */
    _dragRecognizer = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleDragGesture:)];
    _dragRecognizer.minimumPressDuration = 0.2;
    _dragRecognizer.enabled = NO;
    [_scrollView addGestureRecognizer:_dragRecognizer];
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
    [self applyStickyTransforms:NO];
    // A row mounting mid-drag (auto-scroll) must not cover the picked-up row, and
    // needs its make-room shuffle offset applied at once so it appears in the right
    // place rather than flashing in at its resting slot first.
    if (_dragging && _draggedView) {
      [_contentView bringSubviewToFront:_draggedView];
      [self applyDragShuffle];
    }
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
    [self applyStickyTransforms:NO];
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
  _autoHideHeader = NO;
  _autoHideFooter = NO;
  _headerHidden = 0.0;
  _footerHidden = 0.0;
  _lastAutoHideOffset = 0.0;
  _horizontal = NO;
  _contentInsetBottom = 0.0;
  _scrollView.contentInset = UIEdgeInsetsZero;
  _scrollView.verticalScrollIndicatorInsets = UIEdgeInsetsZero;
  _sectionHeaderOverlay = nil;
  _stickyHeaderIndices.clear();
  _stickyHeaderOffsets.clear();
  _stickyHeaderSizes.clear();
  [self teardownDrag];
  _dragEnabled = NO;
  _dragRecognizer.enabled = NO;
  [super prepareForRecycle];
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &nextProps = *std::static_pointer_cast<ShadowlistViewProps const>(props);
  _stickyHeader = nextProps.stickyHeader;
  _stickyFooter = nextProps.stickyFooter;
  _autoHideHeader = nextProps.autoHideHeader;
  _autoHideFooter = nextProps.autoHideFooter;
  _horizontal = nextProps.horizontal;
  _dragEnabled = nextProps.dragEnabled;
  _dragRecognizer.enabled = _dragEnabled;

  [self applyContentInsetBottom:nextProps.contentInsetBottom];

  [super updateProps:props oldProps:oldProps];

  [self applyStickyTransforms:NO];
}

/*
 * Keyboard avoidance. Grow (or shrink) the scroll view's bottom contentInset to the
 * requested value and slide the content up by the same delta so the rows that were
 * behind the keyboard come into view - the "list moves when the keyboard opens"
 * behaviour. The offset shift is clamped to the scrollable range and skipped while a
 * drag owns the offset or on horizontal lists (the inset is vertical-only). Wrapped
 * in a short animation so it tracks the keyboard rather than jumping.
 */
- (void)applyContentInsetBottom:(CGFloat)inset
{
  if (inset < 0) inset = 0;
  if (inset == _contentInsetBottom) return;

  CGFloat delta = inset - _contentInsetBottom;
  _contentInsetBottom = inset;

  if (_horizontal) {
    // The inset is along the vertical (keyboard) axis; a horizontal list has no use
    // for it, but still keep the stored value in sync (handled above) so a later
    // axis flip starts clean.
    return;
  }

  UIEdgeInsets contentInset = _scrollView.contentInset;
  contentInset.bottom = inset;
  UIEdgeInsets indicatorInset = _scrollView.verticalScrollIndicatorInsets;
  indicatorInset.bottom = inset;

  // Follow the keyboard by shifting the offset by the inset delta, clamped so we
  // never scroll past the (newly inset-extended) content. Skipped mid-drag.
  CGPoint offset = _scrollView.contentOffset;
  CGFloat maxOffset = MAX(-inset, _scrollView.contentSize.height - _scrollView.bounds.size.height + inset);
  CGFloat followedY = (_dragging || _dragDropPending)
    ? offset.y
    : MIN(MAX(offset.y + delta, -contentInset.top), maxOffset);

  [UIView animateWithDuration:0.25
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut | UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
    self->_scrollView.contentInset = contentInset;
    self->_scrollView.verticalScrollIndicatorInsets = indicatorInset;
    self->_scrollView.contentOffset = CGPointMake(offset.x, followedY);
  }
                   completion:nil];
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

  /*
   * While a drag is in flight (or settling after a drop) we own the scroll offset, so
   * a core offset correction must not yank the content out from under the finger or
   * jump the list as the reorder lands.
   */
  if (nextStateData.containerOffsetEnabled_ && !_dragging && !_dragDropPending) {
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
  [self applyStickyTransforms:NO];

  /*
   * Mid-drag commit (e.g. an auto-scroll re-virtualization mounted new rows): re-glue
   * the picked-up row to the finger and re-apply the make-room shuffle so freshly
   * mounted siblings are offset too.
   */
  if (_dragging) {
    [self updateDrag];
  }

  /*
   * The post-drop landing (the reorder commit reaching the dropped row's slot, then
   * animating it into place) is detected by the drag category's display-link poll
   * (dropSettleTick), NOT here: a same-size reorder that does not move the viewport top
   * publishes no new state, so this state-commit path would never fire for it and the
   * row would snap via the safety net. See ShadowlistView+DragReorder.
   */
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

  // Advance the auto-hide only on genuine user scrolls (not our own echoed offset).
  [self applyStickyTransforms:userScrolled];
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

#pragma mark - Element helpers

- (NSInteger)indexOfElementView:(UIView *)view
{
  if (![view conformsToProtocol:@protocol(RCTShadowlistElementViewViewProtocol)]) {
    return NSNotFound;
  }
  auto props = std::static_pointer_cast<ShadowlistElementViewProps const>(((UIView<RCTComponentViewProtocol> *)view).props);
  if (!props) {
    return NSNotFound;
  }
  return (NSInteger)props->index;
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
