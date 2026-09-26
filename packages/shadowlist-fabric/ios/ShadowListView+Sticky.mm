#import "ShadowListView.h"
#import "ShadowListView+Internal.h"

/*
 * Sticky pinning for the header, footer and section-header overlay.
 */
/*
 * Move a pinned view along the scroll axis. Skips the write when nothing changed, since
 * pinning runs several times per frame.
 */
static inline void SLSetTranslation(RCTUIView *view, BOOL horizontal, CGFloat translation)
{
  CGAffineTransform transform = horizontal
    ? CGAffineTransformMakeTranslation(translation, 0.0)
    : CGAffineTransformMakeTranslation(0.0, translation);
  if (!CGAffineTransformEqualToTransform(view.transform, transform)) {
    view.transform = transform;
  }
}

/*
 * Show or hide a pinned view, skipping the write when nothing changed.
 */
static inline void SLSetHidden(RCTUIView *view, BOOL hidden)
{
  if (view.hidden != hidden) {
    view.hidden = hidden;
  }
}

@implementation ShadowListView (Sticky)

/*
 * Pin the sticky header and footer to the visible area. The footer lands back in its
 * normal spot when the list is scrolled all the way to the end.
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

  /*
   * Auto hide follows scroll direction. Only user scrolls slide the bar away,
   * a jump from code just resets the starting point.
   */
  CGFloat autoHideDelta = accumulate ? (axisOffset - _lastAutoHideOffset) : 0.0;
  _lastAutoHideOffset = axisOffset;

  if (_stickyHeaderView) {
    CGFloat translation = 0.0;
    if (_autoHideHeader) {
      // Pin to the top, and slide it away once the list scrolls past its height.
      if (axisOffset <= headerSize) {
        _headerHidden = 0.0;
      } else {
        _headerHidden = MAX(0.0, MIN(_headerHidden + autoHideDelta, headerSize));
      }
      translation = axisOffset - _headerHidden;
    } else if (_stickyHeader) {
      // Pin to the top, but let the end of the content or the footer push it off.
      translation = axisOffset;
      CGFloat collisionTop = contentSize - footerSize - headerSize;
      if (collisionTop < translation) {
        translation = collisionTop;
      }
    }
    SLSetTranslation(_stickyHeaderView, _horizontal, translation);
  }

  if (_stickyFooterView) {
    CGFloat translation = 0.0;
    if (_autoHideFooter) {
      // Pin to the bottom, and slide it away unless we are near the end.
      CGFloat maxOffset = MAX(0.0, contentSize - windowSize);
      if (axisOffset >= maxOffset - footerSize) {
        _footerHidden = 0.0;
      } else {
        _footerHidden = MAX(0.0, MIN(_footerHidden + autoHideDelta, footerSize));
      }
      translation = (axisOffset + windowSize - contentSize) + _footerHidden;
    } else if (_stickyFooter) {
      /*
       * Pin to the bottom, but in a short list never ride up into the header's space.
       * Same clamp as the sticky header above.
       */
      translation = axisOffset + windowSize - contentSize;
      CGFloat collisionBottom = headerSize - contentSize + footerSize;
      if (translation < collisionBottom) {
        translation = collisionBottom;
      }
    }
    SLSetTranslation(_stickyFooterView, _horizontal, translation);
  }

  /*
   * Raise the header and footer first so the section header ends up on top. Only needed
   * after something changed the subview order, not on every scroll frame.
   */
  if (_stickyOrderDirty) {
    _stickyOrderDirty = NO;
    [self bringStickyViewsToFront];
    _overlayOrderDirty = YES;
  }
  [self applyStickySectionHeaders];
}

/*
 * Pin the current section header to the top and let the next one push it up.
 * Hide it when no section is active.
 */
- (void)applyStickySectionHeaders
{
  if (!_sectionHeaderOverlay) {
    return;
  }

  if (_stickyHeaderIndices.empty()) {
    SLSetHidden(_sectionHeaderOverlay, YES);
    return;
  }

  CGFloat axisOffset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  if (axisOffset < 0.0) {
    axisOffset = 0.0;
  }

  /*
   * Headers are sorted by offset. The active one is the last at or above the top,
   * and the one after it is the next header that pushes it up.
   */
  bool hasActive = false;
  double activeSize = 0.0;
  bool hasNext = false;
  double nextOffset = 0.0;
  for (std::size_t headerIndex = 0; headerIndex < _stickyHeaderOffsets.size(); ++headerIndex) {
    double headerOffset = _stickyHeaderOffsets[headerIndex];
    if (headerOffset <= axisOffset) {
      hasActive = true;
      activeSize = _stickyHeaderSizes[headerIndex];
    } else {
      nextOffset = headerOffset;
      hasNext = true;
      break;
    }
  }

  if (!hasActive) {
    SLSetHidden(_sectionHeaderOverlay, YES);
    return;
  }

  // Sit at the top, or higher when the next header pushes into it.
  double translation = axisOffset;
  if (hasNext) {
    double pushedTop = nextOffset - activeSize;
    if (pushedTop < translation) {
      translation = pushedTop;
    }
  }

  SLSetHidden(_sectionHeaderOverlay, NO);
  SLSetTranslation(_sectionHeaderOverlay, _horizontal, translation);
  // Sit above the sticky header and footer, which use z 1.
  if (_overlayOrderDirty) {
    _overlayOrderDirty = NO;
    SLRaiseSubview(_contentView, _sectionHeaderOverlay, 2.0);
  }
}

/*
 * Keep a pinned header or footer above the rows. Rows sit at z 0, these at z 1.
 */
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
