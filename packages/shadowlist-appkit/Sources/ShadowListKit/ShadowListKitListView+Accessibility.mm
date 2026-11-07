#import "Internal/ShadowListKitListView+Private.h"
#import "Internal/ShadowListKitSwipeActionsView.h"

#import <ShadowListKit/ShadowListKitListView+Testing.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewAccessibilityLink = 0;

/*
 * NSAccessibilityScrollToVisibleAction, named before macOS 26.
 */
static NSAccessibilityActionName const SHADOWLIST_KIT_SCROLL_TO_VISIBLE_ACTION = @"AXScrollToVisible";

@interface ShadowListKitListView (AccessibilityRows)
- (std::size_t)accessibilityRowOfKey:(NSString *)key;
- (nullable ShadowListKitListCell *)accessibilityCellOfRow:(std::size_t)row;
- (NSRect)accessibilityFrameOfRow:(std::size_t)row;
- (BOOL)isAccessibilityRowSelected:(std::size_t)row;
- (void)setAccessibilityRow:(std::size_t)row selected:(BOOL)selected;
- (BOOL)canSelectAccessibilityRow:(std::size_t)row;
- (NSArray<NSAccessibilityCustomAction *> *)accessibilityMoveActionsForKey:(NSString *)key;
- (void)postAccessibilityNotification:(NSAccessibilityNotificationName)name userInfo:(nullable NSDictionary *)userInfo;
@end

#pragma mark - Row accessibility element

/*
 * Stands in for a row while accessibility walks the list, mounted or not. Its frame is the
 * mounted cell's, or the row's in the core's layout, and the mounted cell is its child.
 * Scrolling it to visible scrolls the row into view.
 */
@interface ShadowListKitRowAccessibilityElement : NSAccessibilityElement
@property (nonatomic, copy) NSString *key;
@property (nonatomic, weak) ShadowListKitListView *list;
@end

// The legacy action calls are the only way to take AXScrollToVisible before macOS 26.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"

@implementation ShadowListKitRowAccessibilityElement

- (std::size_t)row
{
  return [_list accessibilityRowOfKey:_key];
}

- (NSAccessibilityRole)accessibilityRole
{
  return NSAccessibilityRowRole;
}

- (id)accessibilityParent
{
  return _list;
}

- (NSInteger)accessibilityIndex
{
  std::size_t row = [self row];
  return row == UNDEFINED_INDEX ? NSNotFound : (NSInteger)row;
}

- (NSRect)accessibilityFrame
{
  std::size_t row = [self row];
  return row == UNDEFINED_INDEX ? NSZeroRect : [_list accessibilityFrameOfRow:row];
}

- (NSArray *)accessibilityChildren
{
  std::size_t row = [self row];
  ShadowListKitListCell *cell = row == UNDEFINED_INDEX ? nil : [_list accessibilityCellOfRow:row];
  return cell ? NSAccessibilityUnignoredChildren(@[cell]) : @[];
}

- (BOOL)isAccessibilitySelected
{
  std::size_t row = [self row];
  return row != UNDEFINED_INDEX && [_list isAccessibilityRowSelected:row];
}

- (void)setAccessibilitySelected:(BOOL)selected
{
  std::size_t row = [self row];
  if (row != UNDEFINED_INDEX) {
    [_list setAccessibilityRow:row selected:selected];
  }
}

- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector
{
  if (selector == @selector(setAccessibilitySelected:)) {
    std::size_t row = [self row];
    return row != UNDEFINED_INDEX && [_list canSelectAccessibilityRow:row];
  }
  return [super isAccessibilitySelectorAllowed:selector];
}

/*
 * The item's swipe actions, then the moves.
 */
- (NSArray<NSAccessibilityCustomAction *> *)accessibilityCustomActions
{
  std::size_t row = [self row];
  NSInteger item = row == UNDEFINED_INDEX ? NSNotFound : [_list itemForRow:(NSInteger)row];
  NSArray<NSAccessibilityCustomAction *> *swipes = [_list accessibilitySwipeActionsForItem:item];
  NSArray<NSAccessibilityCustomAction *> *moves = [_list accessibilityMoveActionsForKey:_key];
  return swipes.count > 0 ? [swipes arrayByAddingObjectsFromArray:moves] : moves;
}

- (NSArray<NSAccessibilityActionName> *)accessibilityActionNames
{
  return @[SHADOWLIST_KIT_SCROLL_TO_VISIBLE_ACTION];
}

- (void)accessibilityPerformAction:(NSAccessibilityActionName)action
{
  if ([action isEqualToString:SHADOWLIST_KIT_SCROLL_TO_VISIBLE_ACTION]) {
    [_list focusAccessibilityRowForKey:_key];
  }
}

@end

#pragma clang diagnostic pop

#pragma mark - Accessibility

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * The list is a table of one column with every row in it, the way NSTableView shows itself:
 * each row is an element, mounted or not, and a mounted row has its cell as its child. Section
 * header and footer rows are rows too, which leaves their titles in reading order. The header
 * view comes before the rows and the footer view after them. Reading the rows never scrolls the
 * list. Scrolling a row to visible does. A host that gives its own elements, like a grid over a
 * driven list, sets accessibilityElement to NO, which hides the list and its rows.
 */
@implementation ShadowListKitListView (Accessibility)

#pragma mark - List

- (BOOL)isAccessibilityElement
{
  return !_accessibilityHidden;
}

- (void)setAccessibilityElement:(BOOL)accessibilityElement
{
  _accessibilityHidden = !accessibilityElement;
}

- (NSAccessibilityRole)accessibilityRole
{
  return NSAccessibilityTableRole;
}

- (NSArray *)accessibilityChildren
{
  if (_accessibilityHidden) {
    return @[];
  }
  NSMutableArray *children = [NSMutableArray arrayWithCapacity:_driver.getKeyCount() + 4];
  if (_headerView) {
    [children addObjectsFromArray:NSAccessibilityUnignoredChildren(@[_headerView])];
  }
  [children addObjectsFromArray:self.accessibilityRows];
  if (_footerView) {
    [children addObjectsFromArray:NSAccessibilityUnignoredChildren(@[_footerView])];
  }
  if (!_scrollers.hidden) {
    [children addObjectsFromArray:NSAccessibilityUnignoredChildren(@[_scrollers])];
  }
  return children;
}

- (NSArray *)accessibilityRows
{
  if (_accessibilityHidden) {
    return @[];
  }
  std::size_t count = _driver.getKeyCount();
  NSMutableArray *rows = [NSMutableArray arrayWithCapacity:count];
  for (std::size_t row = 0; row < count; ++row) {
    [rows addObject:[self accessibilityElementForRow:row]];
  }
  return rows;
}

/*
 * The rows overlapping the viewport.
 */
- (NSArray *)accessibilityVisibleRows
{
  std::optional<MountedRange> visible = _accessibilityHidden ? std::nullopt : _driver.getVisibleRange();
  NSMutableArray *rows = [NSMutableArray array];
  for (std::size_t row = visible ? visible->low : 0; visible && row <= visible->high && row < _driver.getKeyCount(); ++row) {
    [rows addObject:[self accessibilityElementForRow:row]];
  }
  return rows;
}

- (NSArray *)accessibilitySelectedRows
{
  if (_accessibilityHidden) {
    return @[];
  }
  NSMutableArray *rows = [NSMutableArray array];
  for (std::size_t row : _selection.indicesIn(_driver.getKeys())) {
    [rows addObject:[self accessibilityElementForRow:row]];
  }
  return rows;
}

/*
 * An assistive app selects rows the way the reader does, with the delegate's calls. Rows the
 * delegate refuses and section rows are left out, and one row stays without multiple selection.
 */
- (void)setAccessibilitySelectedRows:(NSArray *)selectedRows
{
  if (!_allowsSelection) {
    return;
  }
  std::vector<std::string> keys;
  for (id element in selectedRows) {
    if (![element isKindOfClass:[ShadowListKitRowAccessibilityElement class]]) {
      continue;
    }
    std::size_t row = [self accessibilityRowOfKey:((ShadowListKitRowAccessibilityElement *)element).key];
    if (row != UNDEFINED_INDEX && [self canSelectAccessibilityRow:row]) {
      keys.push_back(_driver.getKeyAt(row));
    }
  }
  if (!_allowsMultipleSelection && keys.size() > 1) {
    keys.erase(keys.begin() + 1, keys.end());
  }
  [self userSelectOnlyKeys:keys];
}

- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector
{
  if (selector == @selector(setAccessibilitySelectedRows:)) {
    return _allowsSelection;
  }
  return [super isAccessibilitySelectorAllowed:selector];
}

- (NSInteger)accessibilityRowCount
{
  return _accessibilityHidden ? 0 : (NSInteger)_driver.getKeyCount();
}

- (NSInteger)accessibilityColumnCount
{
  return _accessibilityHidden ? 0 : 1;
}

/*
 * A point over a mounted row is its cell's element, or the row's. The pinned row covers the
 * rows under it, and cells fading out after a delete are left out. The buttons of a swiped
 * item show where it moved away from.
 */
- (id)accessibilityHitTest:(NSPoint)point
{
  if (_accessibilityHidden) {
    return NSAccessibilityUnignoredAncestor(self.superview);
  }
  NSRect screen = NSMakeRect(point.x, point.y, 0, 0);
  NSPoint local = self.window ? [self convertRect:[self.window convertRectFromScreen:screen] fromView:nil].origin : NSZeroPoint;
  if (!_scrollers.hidden && [_scrollers hitTest:[self convertPoint:local toView:self.superview]]) {
    return [_scrollers accessibilityHitTest:point];
  }
  NSPoint content = [_contentView convertPoint:local fromView:self];
  if (_swipeActionsView) {
    NSButton *button = [_swipeActionsView buttonAtPoint:[_swipeActionsView convertPoint:local fromView:self]];
    if (button) {
      return [button accessibilityHitTest:point] ?: button;
    }
  }
  ShadowListKitListCell *cell = nil;
  NSView *templates[] = {_headerView, _footerView};
  for (NSView *view : templates) {
    if (view && !view.hidden && NSPointInRect(content, view.frame)) {
      return [view accessibilityHitTest:point] ?: self;
    }
  }
  if (_stickyCell && !_stickyCell.hidden && NSPointInRect(content, _stickyCell.frame)) {
    cell = _stickyCell;
  }
  for (auto entry = _mounted.begin(); !cell && entry != _mounted.end(); ++entry) {
    ShadowListKitListCell *mounted = entry->second;
    if (!mounted.hidden && mounted.mountGeneration != 0 && NSPointInRect(content, mounted.frame)) {
      cell = mounted;
    }
  }
  if (!cell || cell.row == NSNotFound) {
    return self;
  }
  id hit = [cell accessibilityHitTest:point];
  return hit ?: [self accessibilityElementForRow:(std::size_t)cell.row];
}

#pragma mark - Rows

/*
 * The element of a row by its key, made on first use and kept while accessibility holds it.
 */
- (ShadowListKitRowAccessibilityElement *)accessibilityElementForRow:(std::size_t)row
{
  if (!_rowElements) {
    _rowElements = [NSMapTable strongToWeakObjectsMapTable];
  }
  NSString *key = ShadowListKitString(_driver.getKeyAt(row));
  ShadowListKitRowAccessibilityElement *element = [_rowElements objectForKey:key];
  if (!element) {
    element = [ShadowListKitRowAccessibilityElement new];
    element.key = key;
    element.list = self;
    [_rowElements setObject:element forKey:key];
  }
  return element;
}

- (std::size_t)accessibilityRowOfKey:(NSString *)key
{
  std::size_t row = _driver.indexOfKey(ShadowListKitStdString(key));
  return row < _driver.getKeyCount() ? row : UNDEFINED_INDEX;
}

/*
 * The mounted cell of a row. A cell measured ahead and not shown yet does not count, and a cell
 * fading out after a delete is no longer mounted.
 */
- (ShadowListKitListCell *)accessibilityCellOfRow:(std::size_t)row
{
  ShadowListKitListCell *cell = [self mountedCellAtIndex:row];
  return cell && !cell.hidden && cell.mountGeneration != 0 ? cell : nil;
}

- (id)accessibilityParentOfCell:(ShadowListKitListCell *)cell
{
  if (cell.row == NSNotFound || [self accessibilityCellOfRow:(std::size_t)cell.row] != cell) {
    return nil;
  }
  return [self accessibilityElementForRow:(std::size_t)cell.row];
}

/*
 * A mounted row shows where its cell is, pinned or not. A row not placed by the core yet has
 * no frame.
 */
- (NSRect)accessibilityFrameOfRow:(std::size_t)row
{
  ShadowListKitListCell *cell = [self accessibilityCellOfRow:row];
  if (cell) {
    return NSAccessibilityFrameInView(_contentView, cell.frame);
  }
  if (row >= _driver.getRowCount()) {
    return NSZeroRect;
  }
  return NSAccessibilityFrameInView(_contentView, [self rowRect:row]);
}

- (BOOL)isAccessibilityRowSelected:(std::size_t)row
{
  return [self itemForRow:(NSInteger)row] != NSNotFound && _selection.contains(_driver.getKeyAt(row));
}

- (BOOL)canSelectAccessibilityRow:(std::size_t)row
{
  return _allowsSelection && [self shouldSelectRow:row];
}

/*
 * Selecting a row adds it under multiple selection and selects it alone otherwise. Deselecting
 * takes it out.
 */
- (void)setAccessibilityRow:(std::size_t)row selected:(BOOL)selected
{
  if (!_allowsSelection || [self itemForRow:(NSInteger)row] == NSNotFound) {
    return;
  }
  std::string key = _driver.getKeyAt(row);
  if (selected == _selection.contains(key) || (selected && ![self shouldSelectRow:row])) {
    return;
  }
  std::vector<std::string> keys;
  if (_allowsMultipleSelection) {
    keys = _selection.getKeys();
  }
  if (selected) {
    keys.push_back(key);
  } else {
    keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
  }
  [self userSelectOnlyKeys:keys];
}

- (BOOL)isRowOnScreen:(std::size_t)row
{
  if (row >= _driver.getRowCount()) {
    return NO;
  }
  NSEdgeInsets inset = self.adjustedContentInset;
  NSRect bounds = self.bounds;
  NSRect visible = NSMakeRect(NSMinX(bounds) + inset.left, NSMinY(bounds) + inset.top,
    NSWidth(bounds) - inset.left - inset.right, NSHeight(bounds) - inset.top - inset.bottom);
  ShadowListKitListCell *cell = [self accessibilityCellOfRow:row];
  NSRect rect = cell ? cell.frame : [self rowRect:row];
  return NSIntersectsRect([_contentView convertRect:rect toView:self], visible);
}

/*
 * Scroll a row an assistive app moved to into view from the side it comes from, then point
 * accessibility at it.
 */
- (void)focusAccessibilityRowForKey:(NSString *)key
{
  std::size_t row = [self accessibilityRowOfKey:key];
  if (row == UNDEFINED_INDEX) {
    return;
  }
  if (row >= _driver.getRowCount()) {
    // The core has not placed the row yet. Let the next layout pass do it first.
    [self layoutSubtreeIfNeeded];
    row = [self accessibilityRowOfKey:key];
    if (row == UNDEFINED_INDEX || row >= _driver.getRowCount()) {
      return;
    }
  }
  if (![self isRowOnScreen:row]) {
    [self stopScrolling];
    [self revealRow:row];
    [self layoutSubtreeIfNeeded];
  }
  row = [self accessibilityRowOfKey:key];
  if (row == UNDEFINED_INDEX) {
    return;
  }
  [self postAccessibilityNotification:NSAccessibilityLayoutChangedNotification
                             userInfo:@{NSAccessibilityUIElementsKey : @[[self accessibilityElementForRow:row]]}];
}

#pragma mark - Reorder

/*
 * With reorderEnabled an item the delegate lets move has actions that move it one place, the
 * way a drop there would. Along the axis it moves a row of columns, across it in a grid one
 * item.
 */
- (NSArray<NSAccessibilityCustomAction *> *)accessibilityMoveActionsForKey:(NSString *)key
{
  std::size_t row = [self accessibilityRowOfKey:key];
  NSInteger item = row == UNDEFINED_INDEX ? NSNotFound : [self itemForRow:(NSInteger)row];
  if (!_reorderEnabled || item == NSNotFound ||
      ([_delegate respondsToSelector:@selector(listView:canMoveItemAtIndex:)] && ![_delegate listView:self canMoveItemAtIndex:item])) {
    return @[];
  }
  NSInteger columns = MAX(self.numberOfColumns, 1);
  NSArray<NSString *> *along = _horizontal ? @[@"Move left", @"Move right"] : @[@"Move up", @"Move down"];
  NSArray<NSString *> *across = _horizontal ? @[@"Move up", @"Move down"] : @[@"Move left", @"Move right"];
  NSMutableArray<NSAccessibilityCustomAction *> *actions = [NSMutableArray array];
  auto add = [&](NSString *name, NSInteger step) {
    NSInteger fromItem = NSNotFound;
    NSInteger toItem = NSNotFound;
    NSInteger target = (NSInteger)row + step;
    if (target < 0 || target >= (NSInteger)_driver.getKeyCount() ||
        ![self itemsForDropOfRow:row atRow:(std::size_t)target from:&fromItem to:&toItem]) {
      return;
    }
    // The lambda holds the key by reference. The block keeps its own pointer to it.
    NSString *movedKey = key;
    __weak ShadowListKitListView *weakSelf = self;
    [actions addObject:[[NSAccessibilityCustomAction alloc] initWithName:name handler:^BOOL {
      return [weakSelf moveAccessibilityRowForKey:movedKey by:step];
    }]];
  };
  if (columns > 1) {
    add(across[0], -1);
    add(across[1], 1);
  }
  add(along[0], -columns);
  add(along[1], columns);
  return actions;
}

/*
 * Move a row's item the way a drop a step of rows away moves it, then keep it in view and point
 * accessibility at it. Nothing moves while a row is held.
 */
- (BOOL)moveAccessibilityRowForKey:(NSString *)key by:(NSInteger)step
{
  std::size_t row = [self accessibilityRowOfKey:key];
  NSInteger target = row == UNDEFINED_INDEX ? -1 : (NSInteger)row + step;
  NSInteger from = NSNotFound;
  NSInteger to = NSNotFound;
  if (!_reorderEnabled || [self hasHeldRow] || target < 0 || target >= (NSInteger)_driver.getKeyCount() ||
      ![self itemsForDropOfRow:row atRow:(std::size_t)target from:&from to:&to]) {
    return NO;
  }
  [_delegate listView:self moveItemAtIndex:from toIndex:to];
  [self reloadData];
  [self layoutSubtreeIfNeeded];
  [self focusAccessibilityRowForKey:key];
  return YES;
}

#pragma mark - Notifications

/*
 * The selection changes come in a burst, a deselect and a select for one click. Accessibility
 * hears one change once the turn ends.
 */
- (void)selectionChangedForAccessibility
{
  if (_accessibilitySelectionPending) {
    return;
  }
  _accessibilitySelectionPending = YES;
  __weak ShadowListKitListView *weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    ShadowListKitListView *list = weakSelf;
    if (!list) {
      return;
    }
    list->_accessibilitySelectionPending = NO;
    [list postAccessibilityNotification:NSAccessibilitySelectedRowsChangedNotification userInfo:nil];
  });
}

- (void)rowsChangedForAccessibility
{
  std::size_t count = _driver.getKeyCount();
  if (count == _accessibilityRowCount) {
    return;
  }
  _accessibilityRowCount = count;
  [self postAccessibilityNotification:NSAccessibilityRowCountChangedNotification userInfo:nil];
}

/*
 * A page an assistive app scrolled through a scroller. The list says which rows show.
 */
- (void)scrollerViewDidScrollForAccessibility:(ShadowListKitScrollerView *)scrollerView
{
  [self layoutSubtreeIfNeeded];
  [self postAccessibilityNotification:NSAccessibilityLayoutChangedNotification userInfo:nil];
  NSRange visible = self.visibleRange;
  if (visible.location == NSNotFound) {
    return;
  }
  NSString *status = [NSString stringWithFormat:@"Rows %lu to %lu of %lu", (unsigned long)visible.location + 1,
    (unsigned long)NSMaxRange(visible), (unsigned long)_sections.getItemCount()];
  [self postAccessibilityNotification:NSAccessibilityAnnouncementRequestedNotification
                             userInfo:@{NSAccessibilityAnnouncementKey : status,
                               NSAccessibilityPriorityKey : @(NSAccessibilityPriorityMedium)}];
}

- (void)postAccessibilityNotification:(NSAccessibilityNotificationName)name userInfo:(NSDictionary *)userInfo
{
  if (_accessibilityHidden) {
    return;
  }
  if (_accessibilityNotificationHandler) {
    _accessibilityNotificationHandler(name, userInfo);
  }
  if (userInfo) {
    NSAccessibilityPostNotificationWithUserInfo(self, name, userInfo);
  } else {
    NSAccessibilityPostNotification(self, name);
  }
}

@end

#pragma clang diagnostic pop

#pragma mark - Testing

@implementation ShadowListKitListView (AccessibilityTesting)

- (void (^)(NSAccessibilityNotificationName, NSDictionary *))shadowListKit_accessibilityNotificationHandler
{
  return _accessibilityNotificationHandler;
}

- (void)setShadowListKit_accessibilityNotificationHandler:(void (^)(NSAccessibilityNotificationName, NSDictionary *))handler
{
  _accessibilityNotificationHandler = [handler copy];
}

@end
