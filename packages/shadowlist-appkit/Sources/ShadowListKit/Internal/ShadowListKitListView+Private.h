#pragma once

#import <ShadowListKit/ShadowListKitListView.h>
#import <ShadowListKit/ShadowListKitScrollEngine.h>
#import <ShadowListKit/ShadowListKitScrollerView.h>
#import "ShadowListKitCoreBridge.h"
#import "ShadowListKitListCell+Private.h"

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/ListSelection.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>

#include <string>
#include <unordered_map>
#include <vector>

NS_ASSUME_NONNULL_BEGIN

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
extern const char ShadowListKitListViewActionsLink;
extern const char ShadowListKitListViewCommandsLink;
extern const char ShadowListKitListViewDataLink;
extern const char ShadowListKitListViewScrollingLink;
extern const char ShadowListKitListViewSectionsLink;
extern const char ShadowListKitListViewSelectionLink;
}

/*
 * State and helpers the list shares with its categories.
 * _mounted holds the mounted cells by key. A cell follows its key across inserts above it.
 * _sections maps the rows the core places to items, section headers and footers.
 * _windowAlong and _windowCross are the geometry the core last ran with. offset is the scroll
 * offset along the axis, from the content start. writeOffset:byUser: moves to an offset the list
 * asked for itself. Only a move made for the user counts as the user's scrolling. A driven list
 * reports the offset to its delegate instead. recycleCell: hides a cell and puts it back in its
 * reuse pool. settleFrame runs the layout pass the settle display link waited for.
 * mountedCellAtIndex: is the mounted cell of a row, or nil. mountedCellForKey: is the mounted
 * cell of a key, or nil. itemsOfRows: is the items among rows.
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
 */
@interface ShadowListKitListView () {
 @package
  azimgd::shadowlist::ListDriver _driver;
  azimgd::shadowlist::ListSections _sections;
  __weak id<ShadowListKitListViewDataSource> _dataSource;
  __weak id<ShadowListKitListViewDelegate> _delegate;
  BOOL _horizontal;

  std::unordered_map<std::string, ShadowListKitListCell *> _mounted;

  CGFloat _windowAlong;
  CGFloat _windowCross;

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
 * Data changes. structureChanged runs after every change: a waiting saved position lands.
 */
@interface ShadowListKitListView (Data)

- (void)structureChanged;

@end

/*
 * Sections over the rows. itemForRow: and rowForItem: convert indices, NSNotFound when there is
 * none.
 */
@interface ShadowListKitListView (Sections)

- (NSInteger)itemForRow:(NSInteger)row;
- (NSInteger)rowForItem:(NSInteger)item;

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
 */
@interface ShadowListKitListView (Selection)

- (void)applyStateToCell:(ShadowListKitListCell *)cell key:(const std::string&)key;
- (void)clearSelection;
- (void)unhighlight;
- (void)updateHover;
- (void)updateEmphasis;
- (NSInteger)itemIndexOfKey:(const std::string&)key;
- (void)revealRow:(std::size_t)row;

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
