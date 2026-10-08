#import "ShadowListView.h"
#import "ShadowListView+Private.h"

#include <algorithm>

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

  azimgd::shadowlist::StickyInput input;
  input.offset = _horizontal ? offset.x : offset.y;
  input.windowSize = _horizontal ? window.width : window.height;
  input.contentSize = _horizontal ? content.width : content.height;
  input.hasHeader = _stickyHeaderView != nil;
  input.headerSize = _stickyHeaderView
    ? (_horizontal ? _stickyHeaderView.bounds.size.width : _stickyHeaderView.bounds.size.height)
    : 0.0;
  input.stickyHeader = _stickyHeader;
  input.autoHideHeader = _autoHideHeader;
  input.hasFooter = _stickyFooterView != nil;
  input.footerSize = _stickyFooterView
    ? (_horizontal ? _stickyFooterView.bounds.size.width : _stickyFooterView.bounds.size.height)
    : 0.0;
  // The footer rests at the end of the content.
  input.footerStart = input.contentSize - input.footerSize;
  input.stickyFooter = _stickyFooter;
  input.autoHideFooter = _autoHideFooter;
  input.accumulate = accumulate;

  auto translations = azimgd::shadowlist::stickyTranslations(input, _stickyState);
  if (_stickyHeaderView) {
    SLSetTranslation(_stickyHeaderView, _horizontal, translations.header);
  }
  if (_stickyFooterView) {
    SLSetTranslation(_stickyFooterView, _horizontal, translations.footer);
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

  CGFloat axisOffset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  std::size_t count = std::min(_stickyHeaderOffsets.size(), _stickyHeaderSizes.size());
  auto overlay = azimgd::shadowlist::sectionOverlayPosition(
    _stickyHeaderOffsets.data(), _stickyHeaderSizes.data(), _stickyHeaderIndices.empty() ? 0 : count, axisOffset);
  if (!overlay.visible) {
    SLSetHidden(_sectionHeaderOverlay, YES);
    return;
  }

  SLSetHidden(_sectionHeaderOverlay, NO);
  SLSetTranslation(_sectionHeaderOverlay, _horizontal, overlay.translation);
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
