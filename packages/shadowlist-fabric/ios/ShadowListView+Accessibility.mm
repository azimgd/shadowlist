#import "ShadowListView.h"
#import "ShadowListView+Private.h"

#include <shadowlist-core/host/ScrollTarget.hpp>

#include <cmath>
#include <string>

using namespace facebook::react;

#if !TARGET_OS_OSX
@implementation ShadowListScrollView
- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction
{
  ShadowListView *listView = _listView;
  return listView ? [listView accessibilityScrollList:direction] : [super accessibilityScroll:direction];
}
@end

/*
 * Longest wait for the rows of a VoiceOver page scroll before saying which rows show.
 */
static const NSTimeInterval SL_PAGE_ANNOUNCEMENT_MAX_WAIT = 0.3;

/*
 * VoiceOver page scrolling. iOS only.
 */
@implementation ShadowListView (Accessibility)

/*
 * A three finger swipe moves one viewport. Once the rows there mount, VoiceOver says which
 * rows show.
 */
- (BOOL)accessibilityScrollList:(UIAccessibilityScrollDirection)direction
{
  BOOL forward = _horizontal
    ? (direction == UIAccessibilityScrollDirectionLeft || direction == UIAccessibilityScrollDirectionNext)
    : (direction == UIAccessibilityScrollDirectionUp || direction == UIAccessibilityScrollDirectionNext);
  BOOL backward = _horizontal
    ? (direction == UIAccessibilityScrollDirectionRight || direction == UIAccessibilityScrollDirectionPrevious)
    : (direction == UIAccessibilityScrollDirectionDown || direction == UIAccessibilityScrollDirectionPrevious);
  if ((!forward && !backward) || !_state) {
    return NO;
  }
  CGFloat viewport = _horizontal ? _scrollView.bounds.size.width : _scrollView.bounds.size.height;
  CGFloat offset = _horizontal ? _scrollView.contentOffset.x : _scrollView.contentOffset.y;
  CGFloat maxOffset = MAX(0.0, (_horizontal ? _scrollView.contentSize.width : _scrollView.contentSize.height) - viewport);
  CGFloat target = azimgd::shadowlist::pageScrollTarget(offset, viewport, maxOffset, forward ? 1 : -1);
  if (fabs(target - offset) < 1.0) {
    return NO;
  }
  [self stopMomentum];
  _scrollSync.momentumStopped();
  // The page scroll is the user's, like a drag. The core follows it.
  _scrollView.contentOffset = _horizontal ? CGPointMake(target, _scrollView.contentOffset.y)
                                          : CGPointMake(_scrollView.contentOffset.x, target);
  // The jump is over at once. Report the list at rest, or corrections would wait for a gesture end.
  [self clearUserScrolled];
  _pageAnnouncementPending = YES;
  __weak ShadowListView *weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(SL_PAGE_ANNOUNCEMENT_MAX_WAIT * NSEC_PER_SEC)),
    dispatch_get_main_queue(), ^{
      [weakSelf announcePageScroll];
    });
  return YES;
}

/*
 * Say which rows show, like "Rows 4 to 12 of 200". Rows are counted from the keys in props,
 * looked up in an index built once per props.
 */
- (void)announcePageScroll
{
  if (!_pageAnnouncementPending) {
    return;
  }
  _pageAnnouncementPending = NO;
  const auto& props = *std::static_pointer_cast<const ShadowListViewProps>(_props);
  const auto& keys = props.rowKeys;
  if (_pageKeyIndicesProps != _props) {
    // The first row with a key wins, like a search from the start.
    _pageKeyIndices.clear();
    _pageKeyIndices.reserve(keys.size());
    for (std::size_t index = 0; index < keys.size(); ++index) {
      _pageKeyIndices.emplace(keys[index], (NSInteger)index);
    }
    _pageKeyIndicesProps = _props;
  }
  CGRect visible = _scrollView.bounds;
  NSInteger first = NSNotFound;
  NSInteger last = NSNotFound;
  for (RCTUIView *subview in _contentView.subviews) {
    if (subview.hidden || !CGRectIntersectsRect(subview.frame, visible)) {
      continue;
    }
    NSString *key = [self keyOfCellView:subview];
    if (!key) {
      continue;
    }
    auto found = _pageKeyIndices.find(std::string(key.UTF8String));
    if (found == _pageKeyIndices.end()) {
      continue;
    }
    NSInteger index = found->second;
    first = first == NSNotFound ? index : MIN(first, index);
    last = last == NSNotFound ? index : MAX(last, index);
  }
  NSString *status = first == NSNotFound
    ? nil
    : [NSString stringWithFormat:@"Rows %ld to %ld of %lu", (long)first + 1, (long)last + 1, (unsigned long)keys.size()];
  UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification, status);
}

@end
#endif
