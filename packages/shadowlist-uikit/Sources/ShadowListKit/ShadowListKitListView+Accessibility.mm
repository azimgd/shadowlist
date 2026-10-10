#import "Internal/ShadowListKitListView+Private.h"

#include <shadowlist-core/host/ScrollTarget.hpp>

#include <cmath>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewAccessibilityLink = 0;

#pragma mark - Row accessibility element

/*
 * Stands in for a row that is not on screen while accessibility walks the list. Its frame
 * comes from the core's layout. VoiceOver focusing it scrolls the row into view.
 */
@interface ShadowListKitRowAccessibilityElement : UIAccessibilityElement
@property (nonatomic, copy) NSString *key;
@end

@implementation ShadowListKitRowAccessibilityElement

- (void)accessibilityElementDidBecomeFocused
{
  [super accessibilityElementDidBecomeFocused];
  NSString *key = _key;
  __weak ShadowListKitListView *weakList = (ShadowListKitListView *)self.accessibilityContainer;
  // Scroll after the focus change finishes, then hand focus to the mounted cell.
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakList focusAccessibilityRowForKey:key];
  });
}

@end

#pragma mark - Accessibility

/*
 * Accessibility walks every row, not only the mounted ones: the header, then each row, then
 * the footer. A row on screen is its cell. A row off screen is a stand-in at the row's frame.
 * Walking the list never scrolls it. VoiceOver focusing a stand-in scrolls its row into view.
 */
@implementation ShadowListKitListView (Accessibility)

- (NSInteger)accessibilityElementCount
{
  return (NSInteger)_driver.getKeyCount() + (_headerView ? 1 : 0) + (_footerView ? 1 : 0);
}

- (id)accessibilityElementAtIndex:(NSInteger)index
{
  NSInteger rows = (NSInteger)_driver.getKeyCount();
  NSInteger row = index - (_headerView ? 1 : 0);
  if (row < 0) {
    return index == 0 ? _headerView : nil;
  }
  if (row >= rows) {
    return row == rows ? _footerView : nil;
  }
  ShadowListKitListCell *cell = [self mountedCellAtIndex:(std::size_t)row];
  if (cell && !cell.hidden && [self isRowOnScreen:(std::size_t)row]) {
    return cell;
  }
  return [self accessibilityElementForRow:(std::size_t)row];
}

- (ShadowListKitRowAccessibilityElement *)accessibilityElementForRow:(std::size_t)row
{
  if (!_rowElements) {
    _rowElements = [NSMapTable strongToWeakObjectsMapTable];
  }
  NSString *key = ShadowListKitString(_driver.getKeyAt(row));
  ShadowListKitRowAccessibilityElement *element = [_rowElements objectForKey:key];
  if (!element) {
    element = [[ShadowListKitRowAccessibilityElement alloc] initWithAccessibilityContainer:self];
    element.key = key;
    [_rowElements setObject:element forKey:key];
  }
  element.accessibilityFrameInContainerSpace = row < _driver.getCount() ? [self rowRect:row] : CGRectZero;
  return element;
}

/*
 * Scroll a row VoiceOver moved to into view from the side it comes from, then move focus to
 * its cell.
 */
- (void)focusAccessibilityRowForKey:(NSString *)key
{
  std::size_t row = _driver.indexOfKey(ShadowListKitStdString(key));
  if (row == UNDEFINED_INDEX || row >= _driver.getKeyCount()) {
    return;
  }
  if (row >= _driver.getCount()) {
    // The core has not placed the row yet. Let the next layout pass do it first.
    [self layoutIfNeeded];
    row = _driver.indexOfKey(ShadowListKitStdString(key));
    if (row == UNDEFINED_INDEX || row >= _driver.getCount()) {
      return;
    }
  }
  if (![self isRowOnScreen:row]) {
    CGRect visible = UIEdgeInsetsInsetRect(self.bounds, self.adjustedContentInset);
    CGRect rect = [self rowRect:row];
    BOOL before = _horizontal
      ? CGRectGetMinX(rect) < CGRectGetMinX(visible)
      : CGRectGetMinY(rect) < CGRectGetMinY(visible);
    [self scrollToRow:row viewPosition:before ? 0 : 1 animated:NO];
    [self layoutIfNeeded];
  }
  ShadowListKitListCell *cell = [self mountedCellAtIndex:row];
  if (cell) {
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification, cell);
  }
}

- (NSInteger)indexOfAccessibilityElement:(id)element
{
  if ([element isKindOfClass:[ShadowListKitRowAccessibilityElement class]]) {
    std::size_t row = _driver.indexOfKey(ShadowListKitStdString(((ShadowListKitRowAccessibilityElement *)element).key));
    return row == UNDEFINED_INDEX ? NSNotFound : (NSInteger)row + (_headerView ? 1 : 0);
  }
  if (element && element == _headerView) {
    return 0;
  }
  if (element && element == _footerView) {
    return [self accessibilityElementCount] - 1;
  }
  UIView *view = [element isKindOfClass:[UIView class]] ? element : nil;
  while (view && ![view isKindOfClass:[ShadowListKitListCell class]]) {
    view = view.superview;
  }
  ShadowListKitListCell *cell = (ShadowListKitListCell *)view;
  if (!cell || cell.superview != self || cell.row == NSNotFound) {
    return NSNotFound;
  }
  return cell.row + (_headerView ? 1 : 0);
}

- (BOOL)isRowOnScreen:(std::size_t)index
{
  if (index >= _driver.getCount()) {
    return NO;
  }
  CGRect rect = [self rowRect:index];
  CGRect visible = UIEdgeInsetsInsetRect(self.bounds, self.adjustedContentInset);
  return CGRectIntersectsRect(rect, visible);
}

/*
 * A three finger swipe moves one viewport and says which rows show.
 */
- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction
{
  BOOL forward = _horizontal
    ? (direction == UIAccessibilityScrollDirectionLeft || direction == UIAccessibilityScrollDirectionNext)
    : (direction == UIAccessibilityScrollDirectionUp || direction == UIAccessibilityScrollDirectionNext);
  BOOL backward = _horizontal
    ? (direction == UIAccessibilityScrollDirectionRight || direction == UIAccessibilityScrollDirectionPrevious)
    : (direction == UIAccessibilityScrollDirectionDown || direction == UIAccessibilityScrollDirectionPrevious);
  if (!forward && !backward) {
    return NO;
  }
  double offset = [self offset];
  double target = pageScrollTarget(offset, _windowAlong, [self maxOffset], forward ? 1 : -1);
  if (std::fabs(target - offset) < 1) {
    return NO;
  }
  [self stopScrolling];
  [self writeOffset:target byUser:YES];
  [self layoutIfNeeded];
  NSRange visible = self.visibleRange;
  if (visible.location == NSNotFound) {
    UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification, nil);
    return YES;
  }
  NSString *status = [NSString stringWithFormat:@"Rows %lu to %lu of %lu", (unsigned long)visible.location + 1,
    (unsigned long)NSMaxRange(visible), (unsigned long)_sections.getItemCount()];
  UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification, status);
  return YES;
}

@end
