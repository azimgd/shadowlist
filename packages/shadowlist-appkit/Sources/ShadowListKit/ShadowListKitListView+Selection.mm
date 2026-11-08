#import "Internal/ShadowListKitListView+Private.h"

#import <ShadowListKit/ShadowListKitListView+Testing.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewSelectionLink = 0;

/*
 * How long typed characters keep building one type select prefix.
 */
static const NSTimeInterval SHADOWLIST_KIT_TYPE_SELECT_TIMEOUT = 1.0;

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * Selection by key with the mouse and the keyboard, the way NSTableView selects: a click selects
 * when the mouse goes down, the arrow keys move from the row the keyboard is on, the lead, and
 * Shift extends from the anchor. Hover follows the pointer and emphasis the first responder.
 */
@implementation ShadowListKitListView (Selection)

#pragma mark - Selection

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
  BOOL added = _selection.select(key, deselected);
  _selection.setAnchor(key);
  for (const std::string& previous : deselected) {
    [[self mountedCellForKey:previous] setSelected:NO animated:animated];
  }
  ShadowListKitListCell *cell = [self mountedCellForKey:key];
  if (cell && !cell.isSelected) {
    [cell setSelected:YES animated:animated];
  }
  if (added || !deselected.empty()) {
    [self selectionChangedForAccessibility];
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
    [self selectionChangedForAccessibility];
  }
}

- (void)clearSelection
{
  std::vector<std::string> deselected;
  _selection.clear(deselected);
  for (const std::string& key : deselected) {
    [[self mountedCellForKey:key] setSelected:NO animated:NO];
  }
  if (!deselected.empty()) {
    [self selectionChangedForAccessibility];
  }
}

/*
 * A cell coming on screen shows its row's selection and the list's emphasis.
 */
- (void)applyStateToCell:(ShadowListKitListCell *)cell key:(const std::string&)key
{
  BOOL selected = cell.index != NSNotFound && _selection.contains(key);
  if (cell.isSelected != selected) {
    [cell setSelected:selected animated:NO];
  }
  if (cell.isEmphasized != _emphasized) {
    [cell setEmphasized:_emphasized animated:NO];
  }
}

- (NSInteger)itemIndexOfKey:(const std::string&)key
{
  NSInteger row = [self rowOfKey:key];
  return row == NSNotFound ? NSNotFound : [self itemForRow:row];
}

- (NSInteger)rowOfKey:(const std::string&)key
{
  const std::vector<std::string>& keys = _driver.getKeys();
  auto found = std::find(keys.begin(), keys.end(), key);
  return found == keys.end() ? NSNotFound : (NSInteger)(found - keys.begin());
}

- (BOOL)shouldSelectRow:(std::size_t)row
{
  NSInteger item = [self itemForRow:(NSInteger)row];
  if (item == NSNotFound) {
    return NO;
  }
  return ![_delegate respondsToSelector:@selector(listView:shouldSelectItemAtIndex:)] ||
    [_delegate listView:self shouldSelectItemAtIndex:item];
}

#pragma mark - Selection by the reader

/*
 * The reader selected one row. Every other row is deselected and the row becomes the anchor.
 */
- (void)userSelectOnlyRow:(std::size_t)row
{
  if (row >= _driver.getKeyCount() || ![self shouldSelectRow:row]) {
    return;
  }
  // A copy, because the delegate calls below may change the keys.
  std::string key = _driver.getKeyAt(row);
  BOOL wasSelected = _selection.contains(key);
  std::vector<std::string> deselected;
  _selection.selectOnly(key, deselected);
  _selection.setAnchor(key);
  [self userDeselected:deselected];
  if (!wasSelected) {
    [self userSelected:key];
  }
}

/*
 * A Command-click adds a row or takes it out, and makes it the anchor.
 */
- (void)userToggleRow:(std::size_t)row
{
  if (row >= _driver.getKeyCount()) {
    return;
  }
  std::string key = _driver.getKeyAt(row);
  if (_selection.contains(key)) {
    _selection.deselect(key);
    _selection.setAnchor(key);
    [self userDeselected:std::vector<std::string>{key}];
    return;
  }
  if (![self shouldSelectRow:row]) {
    return;
  }
  std::vector<std::string> deselected;
  _selection.select(key, deselected);
  _selection.setAnchor(key);
  [self userDeselected:deselected];
  [self userSelected:key];
}

/*
 * Select the items from the anchor to a row, the new lead. Items the delegate refuses are left
 * out. Without an anchor the row is selected alone. A delegate that changes the data while it is
 * asked ends the range with nothing selected.
 */
- (void)userSelectRangeToRow:(std::size_t)row
{
  if (row >= _driver.getKeyCount()) {
    return;
  }
  NSInteger anchor = _selection.hasAnchor() ? [self rowOfKey:_selection.getAnchorKey()] : NSNotFound;
  if (anchor == NSNotFound || !_allowsMultipleSelection) {
    [self userSelectOnlyRow:row];
    return;
  }
  NSUInteger version = _structureVersion;
  std::string lead = _driver.getKeyAt(row);
  std::vector<std::string> keys;
  std::ptrdiff_t step = (std::size_t)anchor <= row ? 1 : -1;
  for (std::ptrdiff_t current = anchor;; current += step) {
    std::size_t at = (std::size_t)current;
    // A copy, because the delegate call below may change the keys.
    std::string key = _driver.getKeyAt(at);
    if (_selection.contains(key) || [self shouldSelectRow:at]) {
      keys.push_back(std::move(key));
    }
    if (_structureVersion != version) {
      return;
    }
    if (at == row) {
      break;
    }
  }
  if (keys.empty() || keys.back() != lead) {
    return;
  }
  std::unordered_set<std::string> before;
  for (const std::string& key : keys) {
    if (_selection.contains(key)) {
      before.insert(key);
    }
  }
  std::vector<std::string> deselected;
  _selection.selectRange(keys, deselected);
  [self userDeselected:deselected];
  for (const std::string& key : keys) {
    if (before.count(key) == 0 && _selection.contains(key)) {
      [self userSelected:key];
    }
  }
}

/*
 * Every item the delegate allows, up to a data change made while the delegate is asked.
 */
- (void)userSelectAll
{
  NSUInteger version = _structureVersion;
  std::vector<std::string> added;
  std::vector<std::string> ignored;
  for (std::size_t row = 0; row < _driver.getKeyCount(); ++row) {
    std::string key = _driver.getKeyAt(row);
    if (_selection.contains(key)) {
      continue;
    }
    BOOL allowed = [self shouldSelectRow:row];
    if (_structureVersion != version) {
      break;
    }
    if (allowed) {
      _selection.select(key, ignored);
      added.push_back(std::move(key));
    }
  }
  for (const std::string& key : added) {
    [self userSelected:key];
  }
}

- (void)userDeselectAll
{
  std::vector<std::string> deselected;
  _selection.clear(deselected);
  [self userDeselected:deselected];
}

/*
 * Select exactly these keys, the way an assistive app sets the selected rows. The last one
 * becomes the anchor.
 */
- (void)userSelectOnlyKeys:(const std::vector<std::string>&)keys
{
  std::unordered_set<std::string> wanted(keys.begin(), keys.end());
  std::vector<std::string> deselected;
  for (const std::string& key : std::vector<std::string>(_selection.getKeys())) {
    if (wanted.count(key) == 0 && _selection.deselect(key)) {
      deselected.push_back(key);
    }
  }
  std::vector<std::string> added;
  std::vector<std::string> ignored;
  for (const std::string& key : keys) {
    if (!_selection.contains(key) && _selection.select(key, ignored)) {
      added.push_back(key);
    }
  }
  if (!keys.empty()) {
    _selection.setAnchor(keys.back());
  }
  [self userDeselected:deselected];
  for (const std::string& key : added) {
    [self userSelected:key];
  }
}

- (void)userSelected:(const std::string&)key
{
  [self selectionChangedForAccessibility];
  ShadowListKitListCell *cell = [self mountedCellForKey:key];
  if (cell && !cell.isSelected) {
    [cell setSelected:YES animated:YES];
  }
  NSInteger index = [self itemIndexOfKey:key];
  if (index != NSNotFound && [_delegate respondsToSelector:@selector(listView:didSelectItemAtIndex:)]) {
    [_delegate listView:self didSelectItemAtIndex:index];
  }
}

- (void)userDeselected:(const std::vector<std::string>&)keys
{
  if (!keys.empty()) {
    [self selectionChangedForAccessibility];
  }
  BOOL notifies = [_delegate respondsToSelector:@selector(listView:didDeselectItemAtIndex:)];
  for (const std::string& key : keys) {
    ShadowListKitListCell *cell = [self mountedCellForKey:key];
    if (cell.isSelected) {
      [cell setSelected:NO animated:YES];
    }
    NSInteger index = notifies ? [self itemIndexOfKey:key] : NSNotFound;
    if (index != NSNotFound) {
      [_delegate listView:self didDeselectItemAtIndex:index];
    }
  }
}

/*
 * Open the item the keyboard is on when it is selected, otherwise the first selected one.
 */
- (void)activateSelection
{
  NSInteger lead = [self leadRow];
  std::size_t row = UNDEFINED_INDEX;
  if (lead != NSNotFound && _selection.contains(_driver.getKeyAt((std::size_t)lead))) {
    row = (std::size_t)lead;
  } else {
    std::vector<std::size_t> selected = _selection.indicesIn(_driver.getKeys());
    if (!selected.empty()) {
      row = selected.front();
    }
  }
  if (row != UNDEFINED_INDEX) {
    [self activateRow:row];
  }
}

- (void)activateRow:(std::size_t)row
{
  NSInteger item = [self itemForRow:(NSInteger)row];
  if (item != NSNotFound && [_delegate respondsToSelector:@selector(listView:didActivateItemAtIndex:)]) {
    [_delegate listView:self didActivateItemAtIndex:item];
  }
}

#pragma mark - Mouse

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
  return _allowsSelection || _reorderEnabled;
}

/*
 * A click selects when the mouse goes down and highlights the item until it goes up. A
 * Control-click shows the menu. Without selection the click goes to the next responder, unless
 * it pressed an item that can move. A click while an item is swiped open only performs an
 * action or closes it.
 */
- (void)mouseDown:(NSEvent *)event
{
  if ([self swipeOwnsClickAtPoint:[self convertPoint:event.locationInWindow fromView:nil]]) {
    _dragPressed = NO;
    return;
  }
  BOOL pressed = [self pressForDragWithEvent:event];
  if (!_allowsSelection) {
    if (!pressed) {
      [super mouseDown:event];
    }
    return;
  }
  NSEventModifierFlags flags = event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
  if (flags & NSEventModifierFlagControl) {
    NSMenu *menu = [self menuForEvent:event];
    if (menu) {
      [NSMenu popUpContextMenu:menu withEvent:event forView:self];
    }
    return;
  }
  if (self.window.firstResponder != self) {
    [self.window makeFirstResponder:self];
  }
  ShadowListKitListCell *cell = [self itemCellAtPoint:[self convertPoint:event.locationInWindow fromView:nil]];
  BOOL extends = (flags & (NSEventModifierFlagCommand | NSEventModifierFlagShift)) != 0;
  if (!cell || (std::size_t)cell.row >= _driver.getKeyCount()) {
    if (!extends) {
      [self userDeselectAll];
    }
    return;
  }
  std::size_t row = (std::size_t)cell.row;
  _clickedKey = _driver.getKeyAt(row);
  _hasClickedKey = YES;
  ++_clickGeneration;
  if (event.clickCount >= 2) {
    if (!extends) {
      [self activateRow:row];
    }
    return;
  }
  [self highlightCell:cell];
  if (flags & NSEventModifierFlagShift) {
    [self userSelectRangeToRow:row];
  } else if (flags & NSEventModifierFlagCommand) {
    [self userToggleRow:row];
  } else {
    [self userSelectOnlyRow:row];
  }
}

/*
 * The mouse going up drops a held row.
 */
- (void)mouseUp:(NSEvent *)event
{
  BOOL pressed = [self releaseDragPress];
  if (!_allowsSelection) {
    if (!pressed) {
      [super mouseUp:event];
    }
    return;
  }
  [self unhighlight];
  if (!_menuCell) {
    _hasClickedKey = NO;
    _clickedKey.clear();
  }
}

- (void)highlightCell:(ShadowListKitListCell *)cell
{
  if ([_delegate respondsToSelector:@selector(listView:shouldHighlightItemAtIndex:)] &&
      ![_delegate listView:self shouldHighlightItemAtIndex:cell.index]) {
    return;
  }
  [self unhighlight];
  [cell setHighlighted:YES animated:NO];
  _highlightedCell = cell;
}

- (void)unhighlight
{
  ShadowListKitListCell *cell = _highlightedCell;
  _highlightedCell = nil;
  if (cell.isHighlighted) {
    [cell setHighlighted:NO animated:YES];
  }
}

- (NSInteger)clickedIndex
{
  return _hasClickedKey ? [self itemIndexOfKey:_clickedKey] : NSNotFound;
}

#pragma mark - Hover

/*
 * The pointer is followed only while items can be selected.
 */
- (void)updateTrackingAreas
{
  [super updateTrackingAreas];
  if (!_allowsSelection) {
    if (_hoverTracking) {
      [self removeTrackingArea:_hoverTracking];
      _hoverTracking = nil;
    }
    return;
  }
  if (!_hoverTracking) {
    NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited |
      NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect;
    _hoverTracking = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:options owner:self userInfo:nil];
    [self addTrackingArea:_hoverTracking];
  }
}

- (void)mouseMoved:(NSEvent *)event
{
  if (!_allowsSelection) {
    [super mouseMoved:event];
    return;
  }
  _hoverPoint = [self convertPoint:event.locationInWindow fromView:nil];
  _hoverActive = YES;
  [self updateHover];
}

- (void)mouseEntered:(NSEvent *)event
{
  if (event.trackingArea != _hoverTracking) {
    [super mouseEntered:event];
    return;
  }
  [self mouseMoved:event];
}

- (void)mouseExited:(NSEvent *)event
{
  if (event.trackingArea != _hoverTracking) {
    [super mouseExited:event];
    return;
  }
  _hoverActive = NO;
  [self updateHover];
}

/*
 * Hover the item under the pointer. Nothing is hovered while the list scrolls or holds a row.
 */
- (void)updateHover
{
  ShadowListKitListCell *cell = nil;
  if (_hoverActive && _allowsSelection && self.scrollPhase == ShadowListKitScrollPhaseIdle && ![self hasHeldRow]) {
    cell = [self itemCellAtPoint:_hoverPoint];
  }
  ShadowListKitListCell *previous = _hoveredCell;
  if (previous != cell && previous.isHovered) {
    [previous setHovered:NO animated:YES];
  }
  _hoveredCell = cell;
  if (cell && !cell.isHovered) {
    [cell setHovered:YES animated:YES];
  }
}

#pragma mark - First responder

- (BOOL)acceptsFirstResponder
{
  return _allowsSelection;
}

/*
 * A list without selection refuses the first responder, even when asked by hand.
 */
- (BOOL)becomeFirstResponder
{
  if (!_allowsSelection || ![super becomeFirstResponder]) {
    return NO;
  }
  [self updateEmphasisAsFirstResponder:YES];
  return YES;
}

- (BOOL)resignFirstResponder
{
  if (![super resignFirstResponder]) {
    return NO;
  }
  [self updateEmphasisAsFirstResponder:NO];
  return YES;
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
  [super viewWillMoveToWindow:window];
  NSWindow *previous = self.window;
  if (previous) {
    [NSNotificationCenter.defaultCenter removeObserver:self name:NSWindowDidBecomeKeyNotification object:previous];
    [NSNotificationCenter.defaultCenter removeObserver:self name:NSWindowDidResignKeyNotification object:previous];
  }
  if (window) {
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(windowKeyChanged:)
                                               name:NSWindowDidBecomeKeyNotification object:window];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(windowKeyChanged:)
                                               name:NSWindowDidResignKeyNotification object:window];
  }
}

- (void)viewDidMoveToWindow
{
  [super viewDidMoveToWindow];
  _hoverActive = NO;
  [self updateHover];
  [self updateEmphasis];
}

/*
 * The hover tracking area works in the key window only. A window that resigns key with the
 * pointer over a row sends no mouseExited:, and the row stops showing its hover here.
 */
- (void)windowKeyChanged:(NSNotification *)notification
{
  if (!self.window.isKeyWindow && _hoverActive) {
    _hoverActive = NO;
    [self updateHover];
  }
  [self updateEmphasis];
}

- (void)updateEmphasis
{
  [self updateEmphasisAsFirstResponder:self.window.firstResponder == self];
}

/*
 * Cells are emphasized while the list is the key window's first responder. The window names its
 * first responder only after becomeFirstResponder returns.
 */
- (void)updateEmphasisAsFirstResponder:(BOOL)firstResponder
{
  BOOL emphasized = _allowsSelection && firstResponder && self.window.isKeyWindow;
  if (emphasized == _emphasized) {
    return;
  }
  _emphasized = emphasized;
  for (auto& entry : _mounted) {
    ShadowListKitListCell *cell = entry.second;
    if (cell.isEmphasized != emphasized) {
      [cell setEmphasized:emphasized animated:YES];
    }
  }
}

/*
 * The focus ring goes around the list unless focusRingType is none.
 */
- (NSRect)focusRingMaskBounds
{
  return self.focusRingType == NSFocusRingTypeNone ? NSZeroRect : self.bounds;
}

- (void)drawFocusRingMask
{
  if (self.focusRingType != NSFocusRingTypeNone) {
    NSRectFill(self.bounds);
  }
}

#pragma mark - Keys

/*
 * Typing selects by prefix, Space activates and the menu key shows the menu. The rest goes
 * through the key bindings to the commands below. Without selection keys go to the next
 * responder.
 */
- (void)keyDown:(NSEvent *)event
{
  if (!_allowsSelection) {
    [super keyDown:event];
    return;
  }
  if ([self typeSelectWithEvent:event]) {
    return;
  }
  NSEventModifierFlags flags = event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
  BOOL plain = (flags & (NSEventModifierFlagCommand | NSEventModifierFlagControl | NSEventModifierFlagOption)) == 0;
  NSString *characters = event.charactersIgnoringModifiers;
  if (plain && [characters isEqualToString:@" "]) {
    [self activateSelection];
    return;
  }
  if (characters.length == 1 && [characters characterAtIndex:0] == NSMenuFunctionKey) {
    NSInteger lead = [self leadRow];
    if (lead != NSNotFound) {
      [self showMenuForRow:(std::size_t)lead];
    }
    return;
  }
  [self interpretKeyEvents:@[event]];
}

/*
 * The row the keyboard is on: the lead of the selection, or the last row selected.
 */
- (NSInteger)leadRow
{
  if (_selection.hasAnchor()) {
    NSInteger row = [self rowOfKey:_selection.getLeadKey()];
    if (row != NSNotFound) {
      return row;
    }
  }
  if (!_selection.isEmpty()) {
    return [self rowOfKey:_selection.getKeys().back()];
  }
  return NSNotFound;
}

/*
 * The selectable row a step of rows away from the lead, or the next one past it by the same step.
 * The core places a grid's rows, section rows included, in its columns in turn, and a step of
 * numberOfColumns stays in the lead's column. Past the last row there is none. Without a lead a
 * step forward starts at the first item and back at the last.
 */
- (NSInteger)rowStepping:(NSInteger)step
{
  NSInteger count = (NSInteger)_driver.getKeyCount();
  if (count == 0 || step == 0) {
    return NSNotFound;
  }
  NSInteger lead = [self leadRow];
  NSInteger direction = step > 0 ? 1 : -1;
  NSInteger target = lead == NSNotFound ? (step > 0 ? 0 : count - 1) : lead + step;
  NSInteger stride = lead == NSNotFound ? direction : step;
  for (NSInteger row = target; row >= 0 && row < count; row += stride) {
    if ([self shouldSelectRow:(std::size_t)row]) {
      return row;
    }
  }
  return NSNotFound;
}

- (void)moveByRows:(NSInteger)step extending:(BOOL)extending
{
  NSInteger row = [self rowStepping:step];
  if (row != NSNotFound) {
    [self moveToRow:(std::size_t)row extending:extending];
  }
}

/*
 * Go to the first or the last selectable row.
 */
- (void)moveToEnd:(BOOL)end extending:(BOOL)extending
{
  NSInteger count = (NSInteger)_driver.getKeyCount();
  NSInteger direction = end ? -1 : 1;
  for (NSInteger row = end ? count - 1 : 0; row >= 0 && row < count; row += direction) {
    if ([self shouldSelectRow:(std::size_t)row]) {
      [self moveToRow:(std::size_t)row extending:extending];
      return;
    }
  }
}

/*
 * Move a viewport along the axis from the lead.
 */
- (void)moveByPage:(NSInteger)direction extending:(BOOL)extending
{
  NSInteger lead = [self leadRow];
  NSInteger count = (NSInteger)MIN(_driver.getKeyCount(), _driver.getRowCount());
  if (lead == NSNotFound || lead >= count) {
    [self moveToEnd:direction < 0 extending:extending];
    return;
  }
  double start = _driver.getLeadingAt((std::size_t)lead);
  NSInteger row = lead;
  while (row + direction >= 0 && row + direction < count &&
         std::fabs(_driver.getLeadingAt((std::size_t)(row + direction)) - start) <= _windowAlong) {
    row += direction;
  }
  for (; row != lead; row -= direction) {
    if ([self shouldSelectRow:(std::size_t)row]) {
      [self moveToRow:(std::size_t)row extending:extending];
      return;
    }
  }
}

- (void)moveToRow:(std::size_t)row extending:(BOOL)extending
{
  if (extending) {
    [self userSelectRangeToRow:row];
  } else {
    [self userSelectOnlyRow:row];
  }
  [self revealRow:row];
}

/*
 * Scroll a row into view at the nearest edge. A row in view stays where it is.
 */
- (void)revealRow:(std::size_t)row
{
  if (row >= _driver.getRowCount() || _windowAlong <= 0) {
    return;
  }
  double leading = _driver.getLeadingAt(row);
  double trailing = leading + _driver.getExtentAt(row);
  double start = [self offset];
  if (leading < start) {
    [self scrollToRow:row viewPosition:0 animated:NO];
  } else if (trailing > start + _windowAlong) {
    [self scrollToRow:row viewPosition:1 animated:NO];
  }
}

/*
 * An arrow key along the axis steps a row, a grid's row of columns. Across the axis it steps
 * through a grid's columns.
 */
- (void)moveVertically:(BOOL)vertical by:(NSInteger)direction extending:(BOOL)extending
{
  if (vertical != _horizontal) {
    [self moveByRows:direction * self.numberOfColumns extending:extending];
  } else if (self.numberOfColumns > 1) {
    [self moveByRows:direction extending:extending];
  }
}

- (void)moveUp:(id)sender
{
  [self moveVertically:YES by:-1 extending:NO];
}

- (void)moveDown:(id)sender
{
  [self moveVertically:YES by:1 extending:NO];
}

- (void)moveLeft:(id)sender
{
  [self moveVertically:NO by:-1 extending:NO];
}

- (void)moveRight:(id)sender
{
  [self moveVertically:NO by:1 extending:NO];
}

- (void)moveUpAndModifySelection:(id)sender
{
  [self moveVertically:YES by:-1 extending:YES];
}

- (void)moveDownAndModifySelection:(id)sender
{
  [self moveVertically:YES by:1 extending:YES];
}

- (void)moveLeftAndModifySelection:(id)sender
{
  [self moveVertically:NO by:-1 extending:YES];
}

- (void)moveRightAndModifySelection:(id)sender
{
  [self moveVertically:NO by:1 extending:YES];
}

- (void)moveToBeginningOfDocument:(id)sender
{
  [self moveToEnd:NO extending:NO];
}

- (void)moveToEndOfDocument:(id)sender
{
  [self moveToEnd:YES extending:NO];
}

- (void)moveToBeginningOfDocumentAndModifySelection:(id)sender
{
  [self moveToEnd:NO extending:YES];
}

- (void)moveToEndOfDocumentAndModifySelection:(id)sender
{
  [self moveToEnd:YES extending:YES];
}

/*
 * Command with the arrows across a horizontal list goes to its ends.
 */
- (void)moveToLeftEndOfLine:(id)sender
{
  if (_horizontal) {
    [self moveToEnd:NO extending:NO];
  }
}

- (void)moveToRightEndOfLine:(id)sender
{
  if (_horizontal) {
    [self moveToEnd:YES extending:NO];
  }
}

- (void)moveToLeftEndOfLineAndModifySelection:(id)sender
{
  if (_horizontal) {
    [self moveToEnd:NO extending:YES];
  }
}

- (void)moveToRightEndOfLineAndModifySelection:(id)sender
{
  if (_horizontal) {
    [self moveToEnd:YES extending:YES];
  }
}

- (void)pageUp:(id)sender
{
  [self moveByPage:-1 extending:NO];
}

- (void)pageDown:(id)sender
{
  [self moveByPage:1 extending:NO];
}

- (void)pageUpAndModifySelection:(id)sender
{
  [self moveByPage:-1 extending:YES];
}

- (void)pageDownAndModifySelection:(id)sender
{
  [self moveByPage:1 extending:YES];
}

/*
 * Home, End and the page keys scroll and leave the selection.
 */
- (void)scrollPageUp:(id)sender
{
  [self scrollByPage:-1];
}

- (void)scrollPageDown:(id)sender
{
  [self scrollByPage:1];
}

- (void)scrollByPage:(NSInteger)direction
{
  [self stopScrolling];
  double target = std::min(std::max([self offset] + direction * _windowAlong, 0.0), (double)[self maxOffset]);
  [self setContentOffset:[self contentOffsetAt:target] animated:YES];
}

- (void)scrollToBeginningOfDocument:(id)sender
{
  [self stopScrolling];
  [self scrollToStartAnimated:YES];
}

- (void)scrollToEndOfDocument:(id)sender
{
  [self stopScrolling];
  [self scrollToEndAnimated:YES];
}

- (void)insertNewline:(id)sender
{
  [self activateSelection];
}

- (void)selectAll:(id)sender
{
  if (_allowsMultipleSelection) {
    [self userSelectAll];
  }
}

/*
 * Escape puts a held row back, closes a swiped item, or deselects all. With nothing selected it
 * goes on up the responder chain.
 */
- (void)cancelOperation:(id)sender
{
  if ([self hasHeldRow]) {
    [self endDrag:NO];
    return;
  }
  if ([self isSwipeOpen]) {
    [self closeSwipeAnimated:YES];
    return;
  }
  if (_selection.isEmpty()) {
    [self.nextResponder tryToPerform:@selector(cancelOperation:) with:sender];
    return;
  }
  [self userDeselectAll];
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
{
  if (item.action == @selector(selectAll:)) {
    return _allowsSelection && _allowsMultipleSelection && _driver.getKeyCount() > 0;
  }
  return [self respondsToSelector:item.action];
}

#pragma mark - Type select

/*
 * Printable characters without Command or Control select the first item from the lead whose
 * type select text starts with what was typed. A Space starts no prefix.
 */
- (BOOL)typeSelectWithEvent:(NSEvent *)event
{
  if (![_delegate respondsToSelector:@selector(listView:typeSelectStringForItemAtIndex:)]) {
    return NO;
  }
  NSEventModifierFlags flags = event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
  if (flags & (NSEventModifierFlagCommand | NSEventModifierFlagControl | NSEventModifierFlagFunction)) {
    return NO;
  }
  NSString *characters = event.characters;
  if (characters.length == 0) {
    return NO;
  }
  for (NSUInteger index = 0; index < characters.length; ++index) {
    unichar character = [characters characterAtIndex:index];
    if (character < 0x20 || character == 0x7F || (character >= 0xF700 && character <= 0xF8FF)) {
      return NO;
    }
  }
  BOOL continuing = _typeSelectBuffer.length > 0 && event.timestamp - _typeSelectTime <= SHADOWLIST_KIT_TYPE_SELECT_TIMEOUT;
  if (!continuing && [characters isEqualToString:@" "]) {
    return NO;
  }
  _typeSelectBuffer = continuing ? [_typeSelectBuffer stringByAppendingString:characters] : [characters copy];
  _typeSelectTime = event.timestamp;
  [self typeSelect:_typeSelectBuffer];
  return YES;
}

/*
 * The search starts at the lead, which keeps a longer prefix on the same item. The same
 * character typed again moves on to the next item that starts with it.
 */
- (void)typeSelect:(NSString *)prefix
{
  NSInteger count = (NSInteger)_driver.getKeyCount();
  if (count == 0) {
    return;
  }
  NSString *first = [prefix substringToIndex:1];
  BOOL repeated = prefix.length > 1 && [prefix stringByReplacingOccurrencesOfString:first withString:@""].length == 0;
  NSString *match = repeated ? first : prefix;
  NSInteger lead = [self leadRow];
  NSInteger start = lead == NSNotFound ? 0 : lead + (repeated || prefix.length == 1 ? 1 : 0);
  NSStringCompareOptions options = NSAnchoredSearch | NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch;
  for (NSInteger step = 0; step < count; ++step) {
    NSInteger row = (start + step) % count;
    NSInteger item = [self itemForRow:row];
    if (item == NSNotFound) {
      continue;
    }
    NSString *text = [_delegate listView:self typeSelectStringForItemAtIndex:item];
    if (text && [text rangeOfString:match options:options].location != NSNotFound && [self shouldSelectRow:(std::size_t)row]) {
      [self moveToRow:(std::size_t)row extending:NO];
      return;
    }
  }
}

@end

#pragma mark - Testing

@implementation ShadowListKitListView (SelectionTesting)

- (void)shadowListKit_pressKey:(NSString *)characters keyCode:(unsigned short)keyCode modifierFlags:(NSEventModifierFlags)flags
{
  NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:flags
                                   timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:self.window.windowNumber
                                     context:nil characters:characters charactersIgnoringModifiers:characters isARepeat:NO
                                     keyCode:keyCode];
  [self keyDown:event];
  [self layoutSubtreeIfNeeded];
}

- (void)shadowListKit_clickItemAtIndex:(NSInteger)index modifierFlags:(NSEventModifierFlags)flags clickCount:(NSInteger)clickCount
{
  NSPoint point = [self shadowListKit_windowPointOfItemAtIndex:index];
  for (NSEventType type : {NSEventTypeLeftMouseDown, NSEventTypeLeftMouseUp}) {
    [self shadowListKit_sendMouse:type at:point modifierFlags:flags clickCount:clickCount];
  }
  [self layoutSubtreeIfNeeded];
}

- (void)shadowListKit_moveMouseToPoint:(CGPoint)point
{
  NSPoint window = [self convertPoint:point toView:nil];
  [self shadowListKit_sendMouse:NSEventTypeMouseMoved at:window modifierFlags:0 clickCount:0];
}

- (void)shadowListKit_mouseExited
{
  _hoverActive = NO;
  [self updateHover];
}

- (NSPoint)shadowListKit_windowPointOfItemAtIndex:(NSInteger)index
{
  ShadowListKitListCell *cell = [self cellForItemAtIndex:index];
  NSRect frame = cell ? [self convertRect:cell.frame fromView:cell.superview] : NSZeroRect;
  return [self convertPoint:NSMakePoint(NSMidX(frame), NSMidY(frame)) toView:nil];
}

- (void)shadowListKit_sendMouse:(NSEventType)type at:(NSPoint)point modifierFlags:(NSEventModifierFlags)flags clickCount:(NSInteger)clickCount
{
  NSEvent *event = [NSEvent mouseEventWithType:type location:point modifierFlags:flags
                                     timestamp:NSProcessInfo.processInfo.systemUptime
                                  windowNumber:self.window.windowNumber context:nil eventNumber:0 clickCount:clickCount
                                      pressure:type == NSEventTypeLeftMouseDown ? 1 : 0];
  switch (type) {
    case NSEventTypeLeftMouseDown:
      [self mouseDown:event];
      break;
    case NSEventTypeLeftMouseUp:
      [self mouseUp:event];
      break;
    default:
      [self mouseMoved:event];
      break;
  }
}

@end
#pragma clang diagnostic pop
