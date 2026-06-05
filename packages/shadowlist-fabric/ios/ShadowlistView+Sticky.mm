#import "ShadowlistView.h"
#import "ShadowlistView+Internal.h"

/*
 * Native sticky pinning for the header, footer and section-header overlay. Split out
 * of ShadowlistView so the host class keeps to lifecycle/state/scroll; the geometry
 * here runs on the UI thread every scroll tick off the core-published state cached on
 * the host (mirroring Container::resolveStickyHeader).
 */
@implementation ShadowlistView (Sticky)

/*
 * Pin the sticky header/footer to the viewport by translating them relative to
 * their resting frame. The header tracks the scroll offset (stays at the start);
 * the footer tracks offset + window - content so it stays at the viewport end and
 * lands exactly on its resting position at the scroll extreme. Runs on every scroll
 * tick on the UI thread, so it stays in lockstep with the finger.
 */
- (void)applyStickyTransforms:(BOOL)accumulate
{
  CGPoint offset = _scrollView.contentOffset;
  CGSize window = _scrollView.bounds.size;
  CGSize content = _scrollView.contentSize;

  CGFloat axisOffset = _horizontal ? offset.x : offset.y;
  CGFloat windowSize = _horizontal ? window.width : window.height;
  CGFloat contentSize = _horizontal ? content.width : content.height;
  CGFloat headerSize = _stickyHeaderView
    ? (_horizontal ? _stickyHeaderView.bounds.size.width : _stickyHeaderView.bounds.size.height)
    : 0.0;
  CGFloat footerSize = _stickyFooterView
    ? (_horizontal ? _stickyFooterView.bounds.size.width : _stickyFooterView.bounds.size.height)
    : 0.0;

  // Direction-based auto-hide: accumulate the scroll delta into how far the header/
  // footer is slid away. Scrolling toward the content hides it; scrolling back reveals.
  // Only advance on genuine user scrolls (accumulate); a programmatic offset jump
  // (MVCP/scrollToIndex/initial) snaps the reference without sliding the bar.
  CGFloat autoHideDelta = accumulate ? (axisOffset - _lastAutoHideOffset) : 0.0;
  _lastAutoHideOffset = axisOffset;

  if (_stickyHeaderView) {
    CGFloat translation = 0.0;
    if (_autoHideHeader) {
      // Pin to the viewport start, slid up by _headerHidden. Kept fully shown until
      // scrolled past its own height, then hides as you scroll down / reveals up.
      if (axisOffset <= headerSize) {
        _headerHidden = 0.0;
      } else {
        _headerHidden = MAX(0.0, MIN(_headerHidden + autoHideDelta, headerSize));
      }
      translation = axisOffset - _headerHidden;
    } else if (_stickyHeader) {
      // Pin to the viewport start, but let the end of the content (the footer's resting
      // top, or the content end when there is no footer) push the header back off as it
      // scrolls up to meet it - the section-header "pushed off by the next one" motion.
      translation = axisOffset;
      CGFloat collisionTop = contentSize - footerSize - headerSize;
      if (collisionTop < translation) {
        translation = collisionTop;
      }
    }
    _stickyHeaderView.transform = _horizontal
      ? CGAffineTransformMakeTranslation(translation, 0.0)
      : CGAffineTransformMakeTranslation(0.0, translation);
  }

  if (_stickyFooterView) {
    CGFloat translation = 0.0;
    if (_autoHideFooter) {
      // Pin to the viewport end, slid down by _footerHidden. Kept shown near the bottom
      // (visible at the end), then hides as you scroll down / reveals up.
      CGFloat maxOffset = MAX(0.0, contentSize - windowSize);
      if (axisOffset >= maxOffset - footerSize) {
        _footerHidden = 0.0;
      } else {
        _footerHidden = MAX(0.0, MIN(_footerHidden + autoHideDelta, footerSize));
      }
      translation = (axisOffset + windowSize - contentSize) + _footerHidden;
    } else if (_stickyFooter) {
      // The footer always sticks to the viewport end (plain bottom pin).
      translation = axisOffset + windowSize - contentSize;
    }
    _stickyFooterView.transform = _horizontal
      ? CGAffineTransformMakeTranslation(translation, 0.0)
      : CGAffineTransformMakeTranslation(0.0, translation);
  }

  // Raise the plain sticky header/footer first, then the section-header overlay, so
  // when a list has both the active section header (the more important pinned element)
  // stays on top instead of being covered by the sticky list header.
  [self bringStickyViewsToFront];
  [self applyStickySectionHeaders];
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
  if ((_stickyHeader || _autoHideHeader) && _stickyHeaderView) {
    [_contentView bringSubviewToFront:_stickyHeaderView];
  }
  if ((_stickyFooter || _autoHideFooter) && _stickyFooterView) {
    [_contentView bringSubviewToFront:_stickyFooterView];
  }
}

@end
