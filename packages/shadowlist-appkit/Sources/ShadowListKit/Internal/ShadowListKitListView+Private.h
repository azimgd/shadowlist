#pragma once

#import <ShadowListKit/ShadowListKitListView.h>
#import <ShadowListKit/ShadowListKitScrollEngine.h>
#import <ShadowListKit/ShadowListKitScrollerView.h>
#import "ShadowListKitCoreBridge.h"
#import "ShadowListKitListCell+Private.h"

#import <QuartzCore/QuartzCore.h>

#include <shadowlist-core/host/DragReorder.hpp>
#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/ListSelection.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>

#include <string>
#include <unordered_map>
#include <vector>

NS_ASSUME_NONNULL_BEGIN

@class ShadowListKitChangeAnimator;
@class ShadowListKitRowAccessibilityElement;

/*
 * Place a view at a frame with the layer's implicit animations off.
 */
void ShadowListKitPlace(NSView *view, CGRect frame);

/*
 * Each category file of the list defines one of these and ShadowListKitListView.mm refers to
 * all of them. A static library linked without -ObjC loads an archive member only when it
 * defines a referenced symbol. Without these the category objects and their public methods
 * would be left out.
 */
extern "C" {
extern const char ShadowListKitListViewAccessibilityLink;
extern const char ShadowListKitListViewActionsLink;
extern const char ShadowListKitListViewCommandsLink;
extern const char ShadowListKitListViewDataLink;
extern const char ShadowListKitListViewDragLink;
extern const char ShadowListKitListViewScrollingLink;
extern const char ShadowListKitListViewSectionsLink;
extern const char ShadowListKitListViewSelectionLink;
}

/*
 * State and helpers the list shares with its categories.
 * _contentView holds the cells and the header and footer views. Its bounds origin is the
 * content offset.
 * _mounted holds the mounted cells by key. A cell follows its key across inserts above it.
 * _sections maps the rows the core places to items, section headers and footers.
 * _windowAlong and _windowCross are the geometry the core last ran with. offset is the scroll
 * offset along the axis, from the content start. writeOffset:byUser: moves to an offset the list
 * asked for itself. Only a move made for the user counts as the user's scrolling. A driven list
 * reports the offset to its delegate instead. recycleCell: hides a cell and puts it back in its
 * reuse pool. settleFrame runs the layout pass the settle display link waited for.
 * mountedCellAtIndex: is the mounted cell of a row, or nil. mountedCellForKey: is the mounted
 * cell of a key, or nil. itemsOfRows: is the items among rows.
 * _headerView and _footerView are the views before the first row and after the last.
 * _stickyIndices and _stickySectionHeaders pick the rows that pin, and _stickyCell is the cell
 * pinned now.
 * _changes animates data changes and _itemAnimator runs its animations. _changes exists only
 * while animatesChanges is set, which leaves a list without animations no work for them.
 * _pendingAnchor is a saved position waiting for its key. contentOffsetAt: is the content offset
 * of an offset along the axis. invalidateFrame runs the core on the next layout pass.
 * _structureVersion counts data changes, and a mount pass runs again after one. _inLayout is set
 * while AppKit lays the list out. A data change made then, like rows added from the reached
 * callbacks, runs in the layout AppKit already has pending. _sizesFromDataSource is whether the
 * data source gives sizes, read again on every reload. _batch holds the changes
 * performBatchUpdates:completion: collects until its block returns, and _batchPayload the
 * payload of the last reload in it. _contentVersions is the content version of every item
 * applyChanges saw, by key. _engine scrolls the list and _scrollers shows where it is, both
 * unused while the list is driven.
 * _selection holds the selected keys with the anchor of a range, and _highlightedCell the item
 * the mouse presses. _hoveredCell is the item under _hoverPoint, the pointer in the list's
 * coordinates while _hoverActive. _hoverTracking reports the pointer while allowsSelection is
 * set. _emphasized is whether the list is the key window's first responder. _clickedKey is the
 * key of clickedIndex while _hasClickedKey. _menuCell is the cell whose menu is open and
 * _menuOutline the outline drawn over it. _typeSelectBuffer holds the characters typed since
 * _typeSelectTime, the time of the last one.
 * _heldCell is the cell of the held row and _dragTouch the mouse in the list's coordinates.
 * _dragPressed is a mouse press on an item that can move, at _dragStart, which lifts it once
 * the mouse drags it far enough. _dragLink scrolls near the edges, _dragKeyMonitor holds the
 * keys while a row is held and _dragFeedback spaces the haptic ticks of the drop spot.
 * _rowElements holds the rows' accessibility elements by key, kept while accessibility holds
 * them. _accessibilityHidden is set while a host hides the list from accessibility.
 * _accessibilityRowCount is the row count accessibility last heard of, and
 * _accessibilitySelectionPending a selection change waiting to be posted.
 * _accessibilityNotificationHandler hears every notification the list posts, for tests.
 */
@interface ShadowListKitListView () {
 @package
  azimgd::shadowlist::ListDriver _driver;
  azimgd::shadowlist::ListSections _sections;
  __weak id<ShadowListKitListViewDataSource> _dataSource;
  __weak id<ShadowListKitListViewDelegate> _delegate;
  BOOL _horizontal;

  NSView *_contentView;
  std::unordered_map<std::string, ShadowListKitListCell *> _mounted;

  CGFloat _windowAlong;
  CGFloat _windowCross;

  NSView *_headerView;
  NSView *_footerView;
  NSIndexSet *_stickyIndices;
  BOOL _stickySectionHeaders;
  ShadowListKitListCell *_stickyCell;

  BOOL _animatesChanges;
  ShadowListKitChangeAnimator *_changes;
  id<ShadowListKitItemAnimator> _itemAnimator;

  ShadowListKitAnchorState *_pendingAnchor;

  NSUInteger _structureVersion;
  BOOL _inLayout;
  BOOL _sizesFromDataSource;
  azimgd::shadowlist::BatchCollector _batch;
  id _batchPayload;
  azimgd::shadowlist::ContentVersions _contentVersions;
  ShadowListKitScrollEngine *_engine;
  ShadowListKitScrollerView *_scrollers;

  BOOL _allowsSelection;
  BOOL _allowsMultipleSelection;
  azimgd::shadowlist::ListSelection _selection;
  __weak ShadowListKitListCell *_highlightedCell;
  __weak ShadowListKitListCell *_hoveredCell;
  NSPoint _hoverPoint;
  BOOL _hoverActive;
  NSTrackingArea *_hoverTracking;
  BOOL _emphasized;
  std::string _clickedKey;
  BOOL _hasClickedKey;
  __weak ShadowListKitListCell *_menuCell;
  CALayer *_menuOutline;
  NSString *_typeSelectBuffer;
  NSTimeInterval _typeSelectTime;

  BOOL _reorderEnabled;
  ShadowListKitListCell *_heldCell;
  CGPoint _dragTouch;
  CGPoint _dragStart;
  BOOL _dragPressed;
  CADisplayLink *_dragLink;
  id _dragKeyMonitor;
  azimgd::shadowlist::DragFeedback _dragFeedback;

  NSMapTable<NSString *, ShadowListKitRowAccessibilityElement *> *_rowElements;
  BOOL _accessibilityHidden;
  std::size_t _accessibilityRowCount;
  BOOL _accessibilitySelectionPending;
  void (^_accessibilityNotificationHandler)(NSAccessibilityNotificationName, NSDictionary *_Nullable);
}

- (CGFloat)along:(CGPoint)point;
- (CGFloat)cross:(CGPoint)point;
- (CGFloat)leadingInset;
- (double)offset;
- (CGFloat)maxOffset;
- (void)writeOffset:(double)offset byUser:(BOOL)byUser;
- (CGRect)rowRect:(std::size_t)index;
- (void)recycleCell:(ShadowListKitListCell *)cell;
- (void)settleFrame;
- (nullable ShadowListKitListCell *)mountedCellAtIndex:(std::size_t)index;
- (nullable ShadowListKitListCell *)mountedCellForKey:(const std::string&)key;
- (NSIndexSet *)itemsOfRows:(const std::vector<std::size_t>&)rows;
- (CGPoint)contentOffsetAt:(double)offset;
- (void)invalidateFrame;
- (nullable ShadowListKitListCell *)itemCellAtPoint:(NSPoint)point;

@end

/*
 * Scrolling by the wheel, the scrollers and animations. setUpScrolling makes the engine and the
 * scrollers. scrollGeometryChanged hands them the sizes and the offset after a layout pass or an
 * offset change. animateToOffset:landing: animates to an offset along the axis and lands the
 * landing's command through the core when it arrives. claimsScrollWheel: is asked about a wheel
 * event the engine passed on: a gesture across the list's axis or a mouse wheel that moved
 * nothing. Returning NO sends it to the next responder.
 */
@interface ShadowListKitListView (Scrolling) <ShadowListKitScrollEngineDelegate, ShadowListKitScrollerViewDelegate>

- (void)setUpScrolling;
- (void)scrollGeometryChanged;
- (void)animateToOffset:(double)offset landing:(azimgd::shadowlist::ScrollLanding)landing;
- (BOOL)claimsScrollWheel:(NSEvent *)event;

@end

/*
 * Data changes. structureChanged runs after every change: sticky rows follow the sections, a
 * waiting saved position lands and accessibility hears a new row count.
 */
@interface ShadowListKitListView (Data)

- (void)structureChanged;

@end

/*
 * Sections over the rows. itemForRow: and rowForItem: convert indices, NSNotFound when there is
 * none. itemsForDragFromRow:toRow:from:to: maps a drop of a held row to items.
 * updateStickyRows hands the core the pinned rows.
 */
@interface ShadowListKitListView (Sections)

- (NSInteger)itemForRow:(NSInteger)row;
- (NSInteger)rowForItem:(NSInteger)item;
- (BOOL)itemsForDragFromRow:(std::size_t)fromRow toRow:(std::size_t)toRow from:(NSInteger *)from to:(NSInteger *)to;
- (void)updateStickyRows;

@end

/*
 * The saved position and the scroll commands. scrollToRow:viewPosition:animated: scrolls to a
 * row, which can be a section header. stopScrolling stops momentum and cancels a landing.
 * restorePendingAnchor lands a waiting saved position once its key is there.
 * invalidateRestorableStateIfSettled tells the window the saved position changed once the list
 * rests. runCommandNow lays the list out at once for a command the core took.
 */
@interface ShadowListKitListView (Commands)

- (void)scrollToRow:(std::size_t)row viewPosition:(CGFloat)viewPosition animated:(BOOL)animated;
- (void)stopScrolling;
- (void)runCommandNow;
- (void)restorePendingAnchor;
- (void)invalidateRestorableStateIfSettled;

@end

/*
 * Selection by the mouse and the keyboard, hover and emphasis. applyStateToCell:key: gives a
 * cell coming on screen its row's selection and the list's emphasis. clearSelection deselects
 * every row without delegate calls. unhighlight takes the highlight off the pressed item.
 * updateHover hovers the item under the pointer again, after a layout pass moved the content.
 * updateEmphasis follows the window's key state and first responder. itemIndexOfKey: is the item
 * of a key, or NSNotFound. revealRow: scrolls a row into view at the nearest edge.
 * shouldSelectRow: asks the delegate whether a row's item can be selected. userSelectOnlyKeys:
 * selects exactly some keys for the reader, with delegate calls.
 */
@interface ShadowListKitListView (Selection)

- (void)applyStateToCell:(ShadowListKitListCell *)cell key:(const std::string&)key;
- (void)clearSelection;
- (void)unhighlight;
- (void)updateHover;
- (void)updateEmphasis;
- (NSInteger)itemIndexOfKey:(const std::string&)key;
- (void)revealRow:(std::size_t)row;
- (BOOL)shouldSelectRow:(std::size_t)row;
- (void)userSelectOnlyKeys:(const std::vector<std::string>&)keys;

@end

/*
 * Every row as an accessibility element, mounted or not. accessibilityElementForRow: is the
 * element of a row, made on first use. focusAccessibilityRowForKey: scrolls a row into view and
 * points accessibility at it. selectionChangedForAccessibility posts one selection change for
 * all made in a turn. rowsChangedForAccessibility posts a new row count after a data change.
 * accessibilityParentOfCell: is the element of a mounted cell's row, or nil for a cell that is
 * not mounted.
 */
@interface ShadowListKitListView (Accessibility)

- (ShadowListKitRowAccessibilityElement *)accessibilityElementForRow:(std::size_t)row;
- (void)focusAccessibilityRowForKey:(NSString *)key;
- (nullable id)accessibilityParentOfCell:(ShadowListKitListCell *)cell;
- (void)selectionChangedForAccessibility;
- (void)rowsChangedForAccessibility;

@end

/*
 * Drag to reorder with the mouse. hasHeldRow says whether a row is held. pressForDragWithEvent:
 * starts a press on an item that can move and returns whether the list took it.
 * releaseDragPress drops a held row and returns whether the press was the list's. updateDrag
 * places the held row under the mouse and slides the others, after every layout pass too.
 * endDrag: lets go of the held row, moving it in the data with commit. resetDragLookOfCell:
 * takes a recycled cell's drag translation and shadow off. itemsForDropOfRow:atRow:from:to: maps
 * a drop to the items the delegate moves, NO when nothing would move.
 */
@interface ShadowListKitListView (Drag)

- (BOOL)hasHeldRow;
- (BOOL)pressForDragWithEvent:(NSEvent *)event;
- (BOOL)releaseDragPress;
- (void)updateDrag;
- (void)endDrag:(BOOL)commit;
- (void)resetDragLookOfCell:(ShadowListKitListCell *)cell;
- (BOOL)itemsForDropOfRow:(std::size_t)fromRow atRow:(std::size_t)toRow from:(NSInteger *)from to:(NSInteger *)to;

@end

/*
 * Context menus. menuForCell: is the delegate's menu for an item. showMenuForRow: opens it on a
 * row from the keyboard. endMenuTarget takes the outline and clickedIndex off the item once
 * the menu closes.
 */
@interface ShadowListKitListView (Actions)

- (nullable NSMenu *)menuForCell:(ShadowListKitListCell *)cell;
- (void)showMenuForRow:(std::size_t)row;
- (void)endMenuTarget;

@end

NS_ASSUME_NONNULL_END
