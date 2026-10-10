#import "ShadowListView.h"
#import "ShadowListView+Private.h"

using namespace facebook::react;

#if SHADOWLIST_FRAME_TRACE_COMPILED && !TARGET_OS_OSX
/*
 * Run right after Core Animation commits, which uses order 2000000, to see the final frame.
 */
static const CFIndex SL_FRAME_TRACE_OBSERVER_ORDER = 2000001;

static void SLFrameTraceCallback(CFRunLoopObserverRef, CFRunLoopActivity, void *info)
{
  [(__bridge ShadowListView *)info traceFrame];
}

/*
 * Frame trace for debugging scroll jumps, see SLFrameTraceEnabled.
 */
@implementation ShadowListView (FrameTrace)

- (void)startFrameTrace
{
  if (SLFrameTraceEnabled()) {
    CFRunLoopObserverContext context = {0, (__bridge void *)self, NULL, NULL, NULL};
    _frameTraceObserver = CFRunLoopObserverCreate(
      kCFAllocatorDefault, kCFRunLoopBeforeWaiting | kCFRunLoopExit, true, SL_FRAME_TRACE_OBSERVER_ORDER,
      SLFrameTraceCallback, &context);
    CFRunLoopAddObserver(CFRunLoopGetMain(), _frameTraceObserver, kCFRunLoopCommonModes);
  }
}

- (void)stopFrameTrace
{
  if (_frameTraceObserver) {
    CFRunLoopObserverInvalidate(_frameTraceObserver);
    CFRelease(_frameTraceObserver);
  }
}

/*
 * Log one line for each committed frame that changed, with the offset, sizes, header and
 * every visible row. Keys are cut to their last 8 characters since generated ids share a prefix.
 */
- (void)traceFrame
{
  if (!_state || !self.window) {
    return;
  }
  // Everything below is along the scroll axis, y for vertical lists and x for horizontal.
  BOOL horizontal = _horizontal;
  CGFloat offset = horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  CGFloat viewport = horizontal ? _scrollView.bounds.size.width : _scrollView.bounds.size.height;
  auto leading = ^CGFloat(CGRect frame) {
    return horizontal ? CGRectGetMinX(frame) : CGRectGetMinY(frame);
  };
  auto extent = ^CGFloat(CGRect frame) {
    return horizontal ? frame.size.width : frame.size.height;
  };
  NSMutableArray<UIView *> *rows = [NSMutableArray array];
  for (UIView *subview in _contentView.subviews) {
    if (subview.hidden || ![subview conformsToProtocol:@protocol(RCTShadowListElementViewViewProtocol)]) {
      continue;
    }
    CGRect frame = subview.frame;
    if (leading(frame) + extent(frame) <= offset || leading(frame) >= offset + viewport) {
      continue;
    }
    [rows addObject:subview];
  }
  [rows sortUsingComparator:^NSComparisonResult(UIView *a, UIView *b) {
    CGFloat aLeading = leading(a.frame);
    CGFloat bLeading = leading(b.frame);
    return aLeading < bLeading ? NSOrderedAscending : (aLeading > bLeading ? NSOrderedDescending : NSOrderedSame);
  }];
  NSMutableString *rowsDescription = [NSMutableString string];
  for (UIView *row in rows) {
    NSString *key = [self keyOfElementView:row] ?: @"?";
    if (key.length > 8) {
      key = [key substringFromIndex:key.length - 8];
    }
    // A trailing tilde marks a row the layout pass hid with opacity 0.
    [rowsDescription appendFormat:@" %@@%.1f+%.1f%@", key, leading(row.frame) - offset, extent(row.frame),
      row.layer.opacity < 0.5 ? @"~" : @""];
  }
  UIView *header = _stickyHeaderView;
  /*
   * ph is 0 idle, 1 finger down, 2 momentum, 3 scroll to top. ins is the leading inset,
   * which the refresh control adds while spinning. ref is the refreshing prop.
   */
  int phase = _scrollView.isTracking ? 1 : (_scrollView.isDecelerating ? 2 : (_scrollingToTop ? 3 : 0));
  BOOL inverted = std::static_pointer_cast<const ShadowListViewProps>(_props)->inverted;
  // The footer and section overlay, in screen positions like the rows.
  UIView *footer = _stickyFooterView;
  UIView *overlay = _sectionHeaderOverlay;
  BOOL overlayVisible = overlay && !overlay.hidden;
  NSString *signature = [NSString stringWithFormat:@"ax=%@ inv=%d off=%.1f cs=%.1f vp=%.1f ins=%.1f ph=%d ref=%d hdr=%.1f+%.1f ftr=%.1f+%.1f ovl=%.1f+%.1f stt=%d jump=%d rows=[%@ ]",
    horizontal ? @"h" : @"v", inverted ? 1 : 0, offset,
    horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height,
    viewport, horizontal ? _scrollView.adjustedContentInset.left : _scrollView.adjustedContentInset.top, phase,
    _refreshing ? 1 : 0, header ? leading(header.frame) - offset : -1.0, header ? extent(header.frame) : 0.0,
    footer ? leading(footer.frame) - offset : -1.0, footer ? extent(footer.frame) : 0.0,
    overlayVisible ? leading(overlay.frame) - offset : -1.0, overlayVisible ? extent(overlay.frame) : 0.0,
    _scrollingToTop ? 1 : 0, _scrollToTopJumpPending ? 1 : 0, rowsDescription];
  if ([signature isEqualToString:_previousFrameTrace]) {
    return;
  }
  _previousFrameTrace = signature;
  SLF_TRACE("frame %s", signature.UTF8String);
}

@end
#endif
