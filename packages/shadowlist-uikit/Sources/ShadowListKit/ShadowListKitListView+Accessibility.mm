#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitListSupport.h"

#include <shadowlist-core/host/ScrollTarget.hpp>

#include <cmath>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewAccessibilityLink = 0;

/*
 * A header or overlay can be a container. Return its labelled accessible child, respecting
 * an explicit accessibilityElements order instead of announcing an empty container.
 */
static id ShadowListKitHeadingTarget(UIView *view)
{
  if (!view || view.hidden || view.accessibilityElementsHidden) return nil;
  if (view.isAccessibilityElement && view.accessibilityLabel.length > 0) return view;
  NSArray *elements = view.accessibilityElements;
  if (elements) {
    for (id element in elements) {
      id target = [element isKindOfClass:[UIView class]]
        ? ShadowListKitHeadingTarget(element)
        : ([element accessibilityLabel].length > 0 ? element : nil);
      if (target) return target;
    }
    return nil;
  }
  for (UIView *child in view.subviews) {
    id target = ShadowListKitHeadingTarget(child);
    if (target) return target;
  }
  return nil;
}

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
 * the footer. A list without rows has its empty view between the two. A row on screen is its cell. A row off screen is a stand-in at the row's frame.
 * Walking the list never scrolls it. VoiceOver focusing a stand-in scrolls its row into view.
 */
@implementation ShadowListKitListView (Accessibility)

- (NSArray<UIAccessibilityCustomRotor *> *)accessibilityCustomRotors
{
  NSArray<UIAccessibilityCustomRotor *> *custom = [super accessibilityCustomRotors];
  if (custom || !_sections.isSectioned() || _sections.headerRows().empty()) return custom;
  if (!_sectionHeaderRotor) {
    __weak ShadowListKitListView *weakList = self;
    _sectionHeaderRotor = [[UIAccessibilityCustomRotor alloc]
      initWithSystemType:UIAccessibilityCustomSystemRotorTypeHeading
      itemSearchBlock:^UIAccessibilityCustomRotorItemResult *(UIAccessibilityCustomRotorSearchPredicate *predicate) {
        ShadowListKitListView *list = weakList;
        if (!list || !list.window || list.hidden || list.accessibilityElementsHidden) return nil;
        NSInteger current = [list indexOfAccessibilityElement:predicate.currentItem.targetElement];
        BOOL forward = predicate.searchDirection == UIAccessibilityCustomRotorDirectionNext;
        auto headers = list->_sections.headerRows();
        std::vector<std::string> keys;
        for (std::size_t position = 0; position < headers.size(); ++position) {
          std::size_t row = headers[forward ? position : headers.size() - position - 1];
          NSInteger index = (NSInteger)row + (list->_headerView ? 1 : 0);
          if (current != NSNotFound && (forward ? index <= current : index >= current)) continue;
          if (row >= list->_driver.getKeyCount()) return nil;
          keys.push_back(list->_driver.getKeyAt(row));
        }
        for (const std::string& headerKey : keys) {
          std::size_t row = list->_driver.indexOfKey(headerKey);
          if (row == UNDEFINED_INDEX || list->_sections.placeOfRow(row).kind != RowKind::Header) continue;
          NSString *key = ShadowListKitString(headerKey);
          UIView *view = [list scrollAccessibilityRowForKey:key];
          row = list->_driver.indexOfKey(ShadowListKitStdString(key));
          if (row == UNDEFINED_INDEX || list->_sections.placeOfRow(row).kind != RowKind::Header) return nil;
          id target = ShadowListKitHeadingTarget(view);
          if (target) return [[UIAccessibilityCustomRotorItemResult alloc] initWithTargetElement:target targetRange:nil];
        }
        return nil;
      }];
  }
  return @[_sectionHeaderRotor];
}

- (NSInteger)accessibilityElementCount
{
  return [self accessibilityRowCount] + (_headerView ? 1 : 0) + (_footerView ? 1 : 0);
}

/*
 * The elements between the header and the footer: the rows, or the empty view of a list
 * without rows.
 */
- (NSInteger)accessibilityRowCount
{
  UIView *empty = [self shownEmptyView];
  return empty ? 1 : (NSInteger)_driver.getKeyCount();
}

- (id)accessibilityElementAtIndex:(NSInteger)index
{
  NSInteger rows = [self accessibilityRowCount];
  NSInteger row = index - (_headerView ? 1 : 0);
  if (row < 0) {
    return index == 0 ? _headerView : nil;
  }
  if (row >= rows) {
    return row == rows ? _footerView : nil;
  }
  UIView *empty = [self shownEmptyView];
  if (empty) {
    return empty;
  }
  if (_stickyHeaderOverlay && _driver.indexOfKey(_stickyHeaderOverlayKey) == (std::size_t)row) {
    return _stickyHeaderOverlay;
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
  element.accessibilityFrameInContainerSpace = row < _driver.getRowCount() ? [self rowRect:row] : CGRectZero;
  return element;
}

/*
 * Scroll a row VoiceOver moved to into view from the side it comes from, then move focus to
 * its cell.
 */
- (void)focusAccessibilityRowForKey:(NSString *)key
{
  UIView *target = [self scrollAccessibilityRowForKey:key];
  if (target) {
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification, target);
  }
}

- (UIView *)scrollAccessibilityRowForKey:(NSString *)key
{
  std::size_t row = _driver.indexOfKey(ShadowListKitStdString(key));
  if (row == UNDEFINED_INDEX || row >= _driver.getKeyCount()) {
    return nil;
  }
  if (row >= _driver.getRowCount()) {
    // The core has not placed the row yet. Let the next layout pass do it first.
    [self layoutIfNeeded];
    row = _driver.indexOfKey(ShadowListKitStdString(key));
    if (row == UNDEFINED_INDEX || row >= _driver.getRowCount()) {
      return nil;
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
  // A scroll callback can replace the data. Resolve the key again before choosing its view.
  row = _driver.indexOfKey(ShadowListKitStdString(key));
  if (row == UNDEFINED_INDEX || row >= _driver.getRowCount()) return nil;
  ShadowListKitListCell *cell = [self mountedCellAtIndex:row];
  return _stickyHeaderOverlay && _driver.indexOfKey(_stickyHeaderOverlayKey) == row ? _stickyHeaderOverlay : cell;
}

- (NSInteger)indexOfAccessibilityElement:(id)element
{
  UIView *overlayView = [element isKindOfClass:[UIView class]] ? element : nil;
  if (_stickyHeaderOverlay && (overlayView == _stickyHeaderOverlay || [overlayView isDescendantOfView:_stickyHeaderOverlay])) {
    std::size_t row = _driver.indexOfKey(_stickyHeaderOverlayKey);
    return row == UNDEFINED_INDEX ? NSNotFound : (NSInteger)row + (_headerView ? 1 : 0);
  }
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
  if (element && element == [self shownEmptyView]) {
    return _headerView ? 1 : 0;
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
  if (index >= _driver.getRowCount()) {
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
  if (std::fabs(target - offset) < OFFSET_ARRIVED_THRESHOLD) {
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
  NSString *status = ShadowListKitRowsShownAnnouncement(visible, _sections.getItemCount());
  UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification, status);
  return YES;
}

#pragma mark - Notifications

/*
 * VoiceOver reads the rows and their selection again on its own after a data change. The Mac
 * list posts its notifications here.
 */
- (void)selectionChangedForAccessibility
{
}

- (void)rowsChangedForAccessibility
{
}

@end
