#import "ShadowlistView.h"
#import "ShadowlistView+Internal.h"

/* Sticky pinning for the header, footer and section-header overlay. */
@implementation ShadowlistView (Sticky)

/*
 * Pin the sticky header/footer to the viewport. The header tracks the scroll offset;
 * the footer tracks offset + window - content so it lands on its resting position at
 * the scroll extreme.
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

  // Direction-based auto-hide: accumulate the scroll delta into how far the bar is
  // slid away. Only advance on genuine user scrolls; programmatic jumps just reseat
  // the reference.
  CGFloat autoHideDelta = accumulate ? (axisOffset - _lastAutoHideOffset) : 0.0;
  _lastAutoHideOffset = axisOffset;

  if (_stickyHeaderView) {
    CGFloat translation = 0.0;
    if (_autoHideHeader) {
      // Pin to the viewport start, slid up by _headerHidden once scrolled past its
      // own height.
      if (axisOffset <= headerSize) {
        _headerHidden = 0.0;
      } else {
        _headerHidden = MAX(0.0, MIN(_headerHidden + autoHideDelta, headerSize));
      }
      translation = axisOffset - _headerHidden;
    } else if (_stickyHeader) {
      // Pin to the viewport start, but let the content end (or the footer's resting
      // top) push the header back off as it scrolls up to meet it.
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
      // Pin to the viewport end, slid down by _footerHidden except near the bottom.
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

  // Raise the sticky header/footer first, then the section-header overlay, so the
  // active section header stays on top.
  [self bringStickyViewsToFront];
  [self applyStickySectionHeaders];
}

/*
 * Pin the section-header overlay to the viewport start, pushing it up as the next
 * in-flow header scrolls in. Hidden when no section is active.
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

  // Headers ascend by offset: active is the last at/above the viewport start; the
  // first one past it is the "next" that pushes it up.
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

  // Translation is the overlay's displayed top: viewport start, pushed up to
  // nextOffset - activeSize as the next header arrives.
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
  // Above the sticky header/footer (z 1) so the active section header stays on top.
  SLRaiseSubview(_contentView, _sectionHeaderOverlay, 2.0);
}

/* Keep a pinned header/footer above the scrolling elements (z 1; elements sit at 0). */
- (void)bringStickyViewsToFront
{
  if ((_stickyHeader || _autoHideHeader) && _stickyHeaderView) {
    SLRaiseSubview(_contentView, _stickyHeaderView, 1.0);
  }
  if ((_stickyFooter || _autoHideFooter) && _stickyFooterView) {
    SLRaiseSubview(_contentView, _stickyFooterView, 1.0);
  }
}

@end
