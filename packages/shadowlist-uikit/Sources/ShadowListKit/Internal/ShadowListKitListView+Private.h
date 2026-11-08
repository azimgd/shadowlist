#pragma once

#import <ShadowListKit/ShadowListKitListView.h>
#import "ShadowListKitCoreBridge.h"
#import "ShadowListKitListCell+Private.h"
#import "ShadowListKitPlatform.h"

#if TARGET_OS_OSX
#import <ShadowListKit/ShadowListKitScrollEngine.h>
#import <ShadowListKit/ShadowListKitScrollerView.h>
#import <QuartzCore/QuartzCore.h>

#include <shadowlist-core/host/DragReorder.hpp>
#endif
#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/ListSelection.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/SwipeReveal.hpp>

#include <string>
#include <unordered_map>
#include <vector>

NS_ASSUME_NONNULL_BEGIN

@class ShadowListKitChangeAnimator;
@class ShadowListKitRowAccessibilityElement;
@class ShadowListKitSwipeActionsView;
#if !TARGET_OS_OSX
@class ShadowListKitSectionIndexView;
#endif

/*
 * Place a view at a frame. iOS sets its center and bounds, which stay valid while a drag
 * transform is set on it. The Mac sets its frame with the layer's implicit animations off.
 */
void ShadowListKitPlace(ShadowListKitView *view, CGRect frame);

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
#if TARGET_OS_OSX
extern const char ShadowListKitListViewScrollingLink;
#endif
extern const char ShadowListKitListViewSectionsLink;
extern const char ShadowListKitListViewSelectionLink;
}

/*
 * State and helpers the list shares with its categories. Both kits compile this header, and
 * each kit's own state follows the shared state.
 * _mounted holds the mounted cells by key. A cell follows its key across inserts above it.
 * _sections maps the rows the core places to items, section headers and footers.
 * _windowAlong and _windowCross are the geometry the core last ran with. offset is the scroll
 * offset along the axis, from the content start including the header. writeOffset:byUser: moves
 * to an offset the list asked for itself. Only a move made for the user, like the drag auto
 * scroll, counts as the user's scrolling. A driven Mac list reports the offset to its delegate
 * instead. recycleCell: hides a cell and puts it back in its reuse pool. itemCellAtPoint: is the
 * shown item cell under a point in the list's own coordinates. settleFrame runs the layout pass
 * the settle display link waited for.
 * mountedCellAtIndex: is the mounted cell of a row, or nil. mountedCellForKey: is the mounted
 * cell of a key, or nil. itemsOfRows: is the items among rows.
 * _headerView and _footerView are the views before the first row and after the last.
 * _stickyIndices and _stickySectionHeaders pick the rows that pin.
 * _changes animates data changes and _itemAnimator runs its animations.
 * _pendingAnchor is a saved position waiting for its key. contentOffsetAt: is the content offset
 * of an offset along the axis. invalidateFrame runs the core on the next layout pass.
 * _structureVersion counts data changes, and a mount pass runs again after one. _inLayout is set
 * while UIKit or AppKit lays the list out. A data change made then, like rows added from the
 * reached callbacks, runs in the layout already pending. _sizesFromDataSource is whether the
 * data source gives sizes, read again on every reload. _batch holds the changes
 * performBatchUpdates:completion: collects until its block returns, and _batchPayload the
 * payload of the last reload in it. _contentVersions is the content version of every item
 * applyChanges saw, by key.
 * _selection holds the selected keys, on the Mac with the anchor of a range, and
 * _highlightedCell the row a finger rests on or the item the mouse presses.
 * _heldCell is the cell of the held row and _dragTouch the finger or the mouse in the list's
 * coordinates. _dragStart is where the press began. _dragLink scrolls near the edges.
 * _swipeCell is the row swiped open and _swipeActionsView its actions, _swipeOffset how far the
 * row shows moved across the axis and _swipe where it follows and rests.
 * _rowElements holds the rows' accessibility elements by key.
 *
 * iOS: _editing is the list's editing state. _dragPress lifts a row and _dragMoved tells a hold
 * that moved from one that only lifted the row. _swipePan moves a row, _menuInteraction and
 * _editMenu show the menus and _pendingMenu is the menu the edit menu shows next.
 * _sectionIndex is the section index view while the data source gives titles for it.
 * _rowElements keeps the stand-ins of rows off screen while accessibility holds them.
 *
 * Mac: _contentView holds the cells and the header and footer views. Its bounds origin is the
 * content offset. _stickyCell is the cell pinned now. _changes exists only while
 * animatesChanges is set, which leaves a list without animations no work for them. _engine
 * scrolls the list and _scrollers shows where it is, both unused while the list is driven.
 * _hoveredCell is the item under _hoverPoint, the pointer in the list's coordinates while
 * _hoverActive. _hoverTracking reports the pointer while allowsSelection is set. _emphasized is
 * whether the list is the key window's first responder. _clickedKey is the key of clickedIndex
 * while _hasClickedKey, and _clickGeneration counts the clicks and menus that set it. _menuCell
 * is the cell whose menu is open and _menuOutline the outline drawn over it. _typeSelectBuffer
 * holds the characters typed since _typeSelectTime, the time of the last one.
 * _dragPressed is a mouse press on an item that can move, which lifts it once the mouse drags it
 * far enough. _dragKeyMonitor holds the keys while a row is held and _dragFeedback spaces the
 * haptic ticks of the drop spot.
 * _swipesItems is whether the delegate gives swipe actions at all. _swipeTracking is a trackpad
 * gesture moving the row, which has moved it _swipeTranslation at _swipeVelocity, its last event
 * at _swipeTime. _swipePassing is a gesture across the axis the list leaves to the next
 * responder, and _swipeMomentum the momentum after a swipe the list takes. _swipeLink animates
 * the row to _swipeRest from _swipeSettleFrom, _swipeSettleElapsed into the animation.
 * _swipeKeyMonitor holds Escape while a row is open.
 * _rowElements keeps the elements until the key leaves the data. _accessibilityHidden is set
 * while a host hides the list from accessibility. _accessibilityRowCount is the row count
 * accessibility last heard of, and _accessibilitySelectionPending a selection change waiting to
 * be posted. _accessibilityNotificationHandler hears every notification the list posts, for
 * tests.
 */
@interface ShadowListKitListView ()
#if !TARGET_OS_OSX
<UIScrollViewDelegate>
#endif
{
 @package
  azimgd::shadowlist::ListDriver _driver;
  azimgd::shadowlist::ListSections _sections;
  __weak id<ShadowListKitListViewDataSource> _dataSource;
  __weak id<ShadowListKitListViewDelegate> _userDelegate;
  BOOL _horizontal;

  std::unordered_map<std::string, ShadowListKitListCell *> _mounted;

  CGFloat _windowAlong;
  CGFloat _windowCross;

  ShadowListKitView *_headerView;
  ShadowListKitView *_footerView;
  NSIndexSet *_stickyIndices;
  BOOL _stickySectionHeaders;

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

  BOOL _allowsSelection;
  azimgd::shadowlist::ListSelection _selection;
  __weak ShadowListKitListCell *_highlightedCell;

  ShadowListKitListCell *_heldCell;
  CGPoint _dragTouch;
  CGPoint _dragStart;
  CADisplayLink *_dragLink;

  ShadowListKitListCell *_swipeCell;
  ShadowListKitSwipeActionsView *_swipeActionsView;
  azimgd::shadowlist::SwipeReveal _swipe;
  CGFloat _swipeOffset;

  NSMapTable<NSString *, ShadowListKitRowAccessibilityElement *> *_rowElements;

#if TARGET_OS_OSX
  NSView *_contentView;
  ShadowListKitListCell *_stickyCell;
  ShadowListKitScrollEngine *_engine;
  ShadowListKitScrollerView *_scrollers;

  BOOL _allowsMultipleSelection;
  __weak ShadowListKitListCell *_hoveredCell;
  NSPoint _hoverPoint;
  BOOL _hoverActive;
  NSTrackingArea *_hoverTracking;
  BOOL _emphasized;
  std::string _clickedKey;
  BOOL _hasClickedKey;
  NSUInteger _clickGeneration;
  __weak ShadowListKitListCell *_menuCell;
  CALayer *_menuOutline;
  NSString *_typeSelectBuffer;
  NSTimeInterval _typeSelectTime;

  BOOL _reorderEnabled;
  BOOL _dragPressed;
  id _dragKeyMonitor;
  azimgd::shadowlist::DragFeedback _dragFeedback;

  BOOL _swipesItems;
  BOOL _swipeTracking;
  BOOL _swipePassing;
  BOOL _swipeMomentum;
  CGFloat _swipeTranslation;
  CGFloat _swipeVelocity;
  NSTimeInterval _swipeTime;
  CADisplayLink *_swipeLink;
  azimgd::shadowlist::SwipeRest _swipeRest;
  CGFloat _swipeSettleFrom;
  double _swipeSettleElapsed;
  CFTimeInterval _swipeFrameTime;
  id _swipeKeyMonitor;

  BOOL _accessibilityHidden;
  std::size_t _accessibilityRowCount;
  BOOL _accessibilitySelectionPending;
  void (^_accessibilityNotificationHandler)(NSAccessibilityNotificationName, NSDictionary *_Nullable);
#else
  BOOL _editing;

  UILongPressGestureRecognizer *_dragPress;
  BOOL _dragMoved;

  UIPanGestureRecognizer *_swipePan;
  UIContextMenuInteraction *_menuInteraction;
  UIEditMenuInteraction *_editMenu;
  UIMenu *_pendingMenu;

  ShadowListKitSectionIndexView *_sectionIndex;
#endif
}

- (CGFloat)along:(CGPoint)point;
- (CGFloat)cross:(CGPoint)point;
- (CGFloat)leadingInset;
- (double)offset;
- (CGFloat)maxOffset;
- (void)writeOffset:(double)offset byUser:(BOOL)byUser;
- (CGRect)rowRect:(std::size_t)index;
- (void)recycleCell:(ShadowListKitListCell *)cell;
- (nullable ShadowListKitListCell *)itemCellAtPoint:(CGPoint)point;
- (void)settleFrame;
- (nullable ShadowListKitListCell *)mountedCellAtIndex:(std::size_t)index;
- (nullable ShadowListKitListCell *)mountedCellForKey:(const std::string&)key;
- (NSIndexSet *)itemsOfRows:(const std::vector<std::size_t>&)rows;
- (CGPoint)contentOffsetAt:(double)offset;
- (void)invalidateFrame;
#if !TARGET_OS_OSX
- (nullable ShadowListKitListCell *)cellAtPoint:(CGPoint)point;
#endif

@end

#if TARGET_OS_OSX
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
#endif

/*
 * Data changes. structureChanged runs after every change: sticky rows follow the sections, the
 * selection drops deleted rows, a waiting saved position lands and accessibility hears of it.
 */
@interface ShadowListKitListView (Data)

- (void)structureChanged;

@end

/*
 * Sections over the rows. itemForRow: and rowForItem: convert indices, NSNotFound when there is
 * none. itemsForDragFromRow:toRow:from:to: maps a drop of a held row to items.
 * updateStickyRows hands the core the pinned rows. reloadSectionIndex reads the index titles
 * again and layoutSectionIndex keeps the index on the trailing edge. The Mac has no section
 * index and its reloadSectionIndex does nothing.
 */
@interface ShadowListKitListView (Sections)

- (NSInteger)itemForRow:(NSInteger)row;
- (NSInteger)rowForItem:(NSInteger)item;
- (BOOL)itemsForDragFromRow:(std::size_t)fromRow toRow:(std::size_t)toRow from:(NSInteger *)from to:(NSInteger *)to;
- (void)updateStickyRows;
- (void)reloadSectionIndex;
#if !TARGET_OS_OSX
- (void)layoutSectionIndex;
#endif

@end

/*
 * The saved position and the scroll commands. scrollToRow:viewPosition:animated: scrolls to a
 * row, which can be a section header. stopScrolling stops momentum and an animated command.
 * runCommandNow lays the list out at once for a command the core took. restorePendingAnchor
 * lands a waiting saved position once its key is there. invalidateRestorableStateIfSettled tells
 * the Mac window the saved position changed once the list rests.
 */
@interface ShadowListKitListView (Commands)

- (void)scrollToRow:(std::size_t)row viewPosition:(CGFloat)viewPosition animated:(BOOL)animated;
- (void)stopScrolling;
- (void)runCommandNow;
- (void)restorePendingAnchor;
#if TARGET_OS_OSX
- (void)invalidateRestorableStateIfSettled;
#endif

@end

#if TARGET_OS_OSX
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
#else
/*
 * Selection. userSelectedCell: is a tap on a row. clearSelection deselects every row without
 * delegate calls. unhighlight takes the highlight off the touched row.
 */
@interface ShadowListKitListView (Selection)

- (void)userSelectedCell:(ShadowListKitListCell *)cell;
- (void)clearSelection;
- (void)unhighlight;

@end
#endif

/*
 * Every row as an accessibility element. focusAccessibilityRowForKey: scrolls a row into view
 * and points accessibility at it. selectionChangedForAccessibility and
 * rowsChangedForAccessibility hear a data change that dropped selected rows or changed the
 * rows. The Mac posts one selection change for all made in a turn and a new row count, and
 * drops the elements of deleted rows. iOS adds VoiceOver page scrolls.
 * On the Mac, accessibilityElementForRow: is the element of a row, mounted or not, made on first
 * use. accessibilityParentOfCell: is the element of a mounted cell's row, or nil for a cell that
 * is not mounted.
 */
@interface ShadowListKitListView (Accessibility)

- (void)focusAccessibilityRowForKey:(NSString *)key;
- (void)selectionChangedForAccessibility;
- (void)rowsChangedForAccessibility;
#if TARGET_OS_OSX
- (ShadowListKitRowAccessibilityElement *)accessibilityElementForRow:(std::size_t)row;
- (nullable id)accessibilityParentOfCell:(ShadowListKitListCell *)cell;
#endif

@end

#if TARGET_OS_OSX
/*
 * Drag to reorder with the mouse. hasHeldRow says whether a row is held. pressForDragWithEvent:
 * starts a press on an item that can move and returns whether the list took it.
 * releaseDragPress drops a held row and returns whether the press was the list's. updateDrag
 * places the held row under the mouse and slides the others, after every layout pass too.
 * endDrag: lets go of the held row, moving it in the data with commit. adoptHeldCell: lifts the
 * new cell a reload gave the held row. resetDragLookOfCell: takes a recycled cell's drag
 * translation and shadow off. itemsForDropOfRow:atRow:from:to: maps a drop to the items the
 * delegate moves, NO when nothing would move.
 */
@interface ShadowListKitListView (Drag)

- (BOOL)hasHeldRow;
- (BOOL)pressForDragWithEvent:(NSEvent *)event;
- (BOOL)releaseDragPress;
- (void)updateDrag;
- (void)endDrag:(BOOL)commit;
- (void)adoptHeldCell:(ShadowListKitListCell *)cell;
- (void)resetDragLookOfCell:(ShadowListKitListCell *)cell;
- (BOOL)itemsForDropOfRow:(std::size_t)fromRow atRow:(std::size_t)toRow from:(NSInteger *)from to:(NSInteger *)to;

@end

/*
 * Swipe actions and context menus. updateSwipesItems reads whether the delegate gives swipe
 * actions. swipeWithScrollWheel: moves a row with a trackpad gesture across the axis the engine
 * passed on and returns whether the list takes the event. isSwipeOpen says whether a row is
 * open, and isSwipedOutCell: whether a cell slid all the way out. closeSwipeAnimated: closes it.
 * swipeOwnsClickAtPoint: takes a click while a row is open: on an action it performs it,
 * anywhere else it closes the row. layoutSwipe keeps the actions under the swiped row after a
 * layout pass. swipeCellWillRecycle: drops the swipe of a cell going back to the pool.
 * accessibilitySwipeActionsForItem: is an item's swipe actions as accessibility actions.
 * menuForCell: is the delegate's menu for an item, or a menu of its swipe actions.
 * showMenuForRow: opens it on a row from the keyboard. endMenuTarget takes the outline and
 * clickedIndex off the item once the menu closes.
 */
@interface ShadowListKitListView (Actions)

- (void)updateSwipesItems;
- (BOOL)swipeWithScrollWheel:(NSEvent *)event;
- (BOOL)isSwipeOpen;
- (BOOL)isSwipedOutCell:(ShadowListKitListCell *)cell;
- (void)closeSwipeAnimated:(BOOL)animated;
- (BOOL)swipeOwnsClickAtPoint:(NSPoint)point;
- (void)layoutSwipe;
- (void)swipeCellWillRecycle:(ShadowListKitListCell *)cell;
- (NSArray<NSAccessibilityCustomAction *> *)accessibilitySwipeActionsForItem:(NSInteger)item;
- (nullable NSMenu *)menuForCell:(ShadowListKitListCell *)cell;
- (void)showMenuForRow:(std::size_t)row;
- (void)endMenuTarget;

@end
#else
/*
 * hasHeldRow says whether a row is held. UIScrollView's isDragging is the scroll gesture, not
 * this. adoptHeldCell: lifts the new cell a reload gave the held row.
 */
@interface ShadowListKitListView (Drag)

- (BOOL)hasHeldRow;
- (void)adoptHeldCell:(ShadowListKitListCell *)cell;
- (void)enableDragPress:(BOOL)enabled;
- (void)applyDragShiftsAnimated:(BOOL)animated;

@end

/*
 * Swipe actions and context menus. installActionGestures runs once at init. layoutSwipe keeps
 * the actions under the swiped row after a layout pass. closeSwipeAnimated: closes an open row.
 * swipeCellWillRecycle: drops the swipe of a cell going back to the pool.
 * showMenuForCell:atPoint: shows a held row's menu.
 */
@interface ShadowListKitListView (Actions) <UIContextMenuInteractionDelegate, UIEditMenuInteractionDelegate>

- (void)installActionGestures;
- (void)layoutSwipe;
- (BOOL)isSwipeOpen;
- (void)closeSwipeAnimated:(BOOL)animated;
- (BOOL)swipeOwnsTapAtPoint:(CGPoint)point;
- (BOOL)isSwipedOutCell:(ShadowListKitListCell *)cell;
- (void)swipeCellWillRecycle:(ShadowListKitListCell *)cell;
- (BOOL)shouldBeginActionGesture:(UIGestureRecognizer *)gesture;
- (BOOL)swipeTakesPan:(UIPanGestureRecognizer *)pan;
- (BOOL)showMenuForCell:(ShadowListKitListCell *)cell atPoint:(CGPoint)point;
- (void)scriptSwipeOfCell:(ShadowListKitListCell *)cell distance:(CGFloat)distance velocity:(CGFloat)velocity;

@end
#endif

NS_ASSUME_NONNULL_END
