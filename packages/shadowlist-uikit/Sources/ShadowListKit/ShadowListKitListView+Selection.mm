#import "Internal/ShadowListKitListView+Private.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewSelectionLink = 0;

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * Selection by key: taps, the selection calls and the highlight of a touched row.
 */
@implementation ShadowListKitListView (Selection)

/*
 * A tap on a row. Multiple selection toggles it, single selection moves to it.
 */
- (void)userSelectedCell:(ShadowListKitListCell *)cell
{
  // A data change since the last layout pass may have dropped the cell's row.
  if ((std::size_t)cell.row >= _driver.getKeyCount()) {
    return;
  }
  NSInteger index = cell.index;
  // A copy, because the delegate calls below may change the keys.
  std::string key = _driver.getKeyAt((std::size_t)cell.row);
  id<ShadowListKitListViewDelegate> delegate = _userDelegate;
  SelectionTap tap = _selection.tap(key, [&] {
    return ![delegate respondsToSelector:@selector(listView:shouldSelectItemAtIndex:)] ||
      [delegate listView:self shouldSelectItemAtIndex:index];
  });
  if (tap.toggledOff) {
    [cell setSelected:NO animated:YES];
    if ([delegate respondsToSelector:@selector(listView:didDeselectItemAtIndex:)]) {
      [delegate listView:self didDeselectItemAtIndex:index];
    }
    return;
  }
  if (!tap.selected) {
    return;
  }
  for (const std::string& previous : tap.deselected) {
    ShadowListKitListCell *previousCell = [self mountedCellForKey:previous];
    [previousCell setSelected:NO animated:YES];
    NSInteger previousIndex = [self itemIndexOfKey:previous];
    if (previousIndex != NSNotFound && [delegate respondsToSelector:@selector(listView:didDeselectItemAtIndex:)]) {
      [delegate listView:self didDeselectItemAtIndex:previousIndex];
    }
  }
  if (!cell.isSelected) {
    [cell setSelected:YES animated:YES];
  }
  if ([delegate respondsToSelector:@selector(listView:didSelectItemAtIndex:)]) {
    [delegate listView:self didSelectItemAtIndex:index];
  }
}

- (NSInteger)itemIndexOfKey:(const std::string&)key
{
  const std::vector<std::string>& keys = _driver.getKeys();
  auto found = std::find(keys.begin(), keys.end(), key);
  return found == keys.end() ? NSNotFound : [self itemForRow:(NSInteger)(found - keys.begin())];
}

- (NSIndexSet *)selectedIndices
{
  return [self itemsOfRows:_selection.indicesIn(_driver.getKeys())];
}

- (void)selectItemAtIndex:(NSInteger)index animated:(BOOL)animated
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound || !_allowsSelection) {
    return;
  }
  std::string key = _driver.getKeyAt((std::size_t)row);
  std::vector<std::string> deselected;
  _selection.select(key, deselected);
  for (const std::string& previous : deselected) {
    [[self mountedCellForKey:previous] setSelected:NO animated:animated];
  }
  ShadowListKitListCell *cell = [self mountedCellForKey:key];
  if (cell && !cell.isSelected) {
    [cell setSelected:YES animated:animated];
  }
}

- (void)deselectItemAtIndex:(NSInteger)index animated:(BOOL)animated
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound) {
    return;
  }
  std::string key = _driver.getKeyAt((std::size_t)row);
  if (_selection.deselect(key)) {
    [[self mountedCellForKey:key] setSelected:NO animated:animated];
  }
}

- (void)clearSelection
{
  std::vector<std::string> deselected;
  _selection.clear(deselected);
  for (const std::string& key : deselected) {
    [[self mountedCellForKey:key] setSelected:NO animated:NO];
  }
}

/*
 * A finger resting on a selectable row highlights it. UIScrollView delays the touch until it
 * knows the finger is not scrolling, and cancels it when it scrolls after all.
 */
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesBegan:touches withEvent:event];
  if (!_allowsSelection || touches.count != 1 || [self hasHeldRow] || [self isSwipeOpen]) {
    return;
  }
  ShadowListKitListCell *cell = [self itemCellAtPoint:[touches.anyObject locationInView:self]];
  if (!cell || ([_userDelegate respondsToSelector:@selector(listView:shouldHighlightItemAtIndex:)] &&
                ![_userDelegate listView:self shouldHighlightItemAtIndex:cell.index])) {
    return;
  }
  [cell setHighlighted:YES animated:NO];
  _highlightedCell = cell;
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesEnded:touches withEvent:event];
  [self unhighlight];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesCancelled:touches withEvent:event];
  [self unhighlight];
}

- (void)unhighlight
{
  ShadowListKitListCell *cell = _highlightedCell;
  _highlightedCell = nil;
  if (cell.isHighlighted) {
    [cell setHighlighted:NO animated:YES];
  }
}

@end
#pragma clang diagnostic pop
