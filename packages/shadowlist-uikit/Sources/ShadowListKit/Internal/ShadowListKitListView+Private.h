#import <ShadowListKit/ShadowListKitListView.h>
#import "ShadowListKitListCell+Private.h"

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
@class ShadowListKitSwipeActionsView;
@class ShadowListKitRowAccessibilityElement;
@class ShadowListKitSectionIndexView;

/*
 * Place a view at a frame through its center and bounds, which stay valid while a drag
 * transform is set on it.
 */
void ShadowListKitPlace(UIView *view, CGRect frame);

/*
 * Convert between keys the core holds and strings. A nil string becomes an empty key, and bytes
 * that are not UTF-8 become an empty string.
 */
NSString *ShadowListKitString(const std::string& value);
std::string ShadowListKitStdString(NSString *_Nullable string);

/*
 * The indices in a set, low to high.
 */
std::vector<std::size_t> ShadowListKitIndices(NSIndexSet *set);

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
extern const char ShadowListKitListViewSectionsLink;
extern const char ShadowListKitListViewSelectionLink;
}

/*
 * State and helpers the list shares with its categories in ShadowListKitListView+Drag.mm and
 * ShadowListKitListView+Actions.mm.
 * _mounted holds the mounted cells by key. A cell follows its key across inserts above it.
 * _sections maps the rows the core places to items, section headers and footers.
 * _windowAlong and _windowCross are the geometry the core last ran with. _dragPress, _heldCell
 * and _dragTouch are the drag in progress: the held row's cell and where the finger is in the
 * viewport. _dragStart and _dragMoved tell a hold that moved from one that only lifted the row.
 * _swipe* is the row swiped open and its actions. offset is the scroll offset along the axis,
 * from the content start including the header. writeOffset:byUser: moves to an offset the list
 * asked for itself. Only a move made for the user, like the drag auto scroll, counts as the
 * user's scrolling. recycleCell: hides a cell and puts it back in its reuse pool. cellAtPoint:
 * is the visible cell under a point in the list's own coordinates. itemCellAtPoint: is the same
 * for item cells only.
 * settleFrame runs the layout pass the settle display link waited for.
 * _rowElements holds the stand-ins of rows off screen by key, kept while accessibility holds them.
 * mountedCellAtIndex: is the mounted cell of a row, or nil.
 * _sectionIndex is the section index view while the data source gives titles for it.
 * _selection holds the selected keys and _highlightedCell the row a finger rests on.
 * mountedCellForKey: is the mounted cell of a key, or nil. itemsOfRows: is the items among rows.
 * _pendingAnchor is a saved position waiting for its key. contentOffsetAt: is the content offset
 * of an offset along the axis. invalidateFrame runs the core on the next layout pass.
 * _changes animates data changes and _itemAnimator runs its animations. _structureVersion counts
 * data changes, and a mount pass runs again after one. _inLayoutSubviews is set while UIKit lays
 * the list out. A data change made then, like rows added from the reached callbacks, runs in the
 * layout UIKit already has pending. _sizesFromDataSource is whether the data source gives sizes,
 * read again on every reload. _batch* holds the changes performBatchUpdates:completion: collects
 * until its block returns. _contentVersions is the content version of every item applyChanges
 * saw, by key.
 */
@interface ShadowListKitListView () <UIScrollViewDelegate> {
 @package
  azimgd::shadowlist::ListDriver _driver;
  azimgd::shadowlist::ListSections _sections;
  __weak id<ShadowListKitListViewDataSource> _dataSource;
  __weak id<ShadowListKitListViewDelegate> _userDelegate;
  BOOL _horizontal;
  BOOL _animatesChanges;
  BOOL _editing;

  std::unordered_map<std::string, ShadowListKitListCell *> _mounted;

  CGFloat _windowAlong;
  CGFloat _windowCross;

  UILongPressGestureRecognizer *_dragPress;
  ShadowListKitListCell *_heldCell;
  CGPoint _dragTouch;
  CGPoint _dragStart;
  BOOL _dragMoved;
  CADisplayLink *_dragLink;

  UIPanGestureRecognizer *_swipePan;
  ShadowListKitListCell *_swipeCell;
  ShadowListKitSwipeActionsView *_swipeActionsView;
  azimgd::shadowlist::SwipeReveal _swipe;
  CGFloat _swipeOffset;
  UIContextMenuInteraction *_menuInteraction;
  UIEditMenuInteraction *_editMenu;
  UIMenu *_pendingMenu;

  UIView *_headerView;
  UIView *_footerView;
  NSMapTable<NSString *, ShadowListKitRowAccessibilityElement *> *_rowElements;

  NSIndexSet *_stickyIndices;
  BOOL _stickySectionHeaders;
  ShadowListKitSectionIndexView *_sectionIndex;

  BOOL _allowsSelection;
  azimgd::shadowlist::ListSelection _selection;
  __weak ShadowListKitListCell *_highlightedCell;

  ShadowListKitAnchorState *_pendingAnchor;

  ShadowListKitChangeAnimator *_changes;
  id<ShadowListKitItemAnimator> _itemAnimator;
  NSUInteger _structureVersion;
  BOOL _inLayoutSubviews;
  BOOL _sizesFromDataSource;
  NSInteger _batchDepth;
  azimgd::shadowlist::BatchUpdate _batch;
  BOOL _batchNeedsReload;
  id _batchPayload;
  azimgd::shadowlist::ContentVersions _contentVersions;
}

- (CGFloat)along:(CGPoint)point;
- (CGFloat)cross:(CGPoint)point;
- (CGFloat)leadingInset;
- (double)offset;
- (CGFloat)maxOffset;
- (void)writeOffset:(double)offset byUser:(BOOL)byUser;
- (CGRect)rowRect:(std::size_t)index;
- (void)recycleCell:(ShadowListKitListCell *)cell;
- (nullable ShadowListKitListCell *)cellAtPoint:(CGPoint)point;
- (nullable ShadowListKitListCell *)itemCellAtPoint:(CGPoint)point;
- (void)settleFrame;
- (nullable ShadowListKitListCell *)mountedCellAtIndex:(std::size_t)index;
- (nullable ShadowListKitListCell *)mountedCellForKey:(const std::string&)key;
- (NSIndexSet *)itemsOfRows:(const std::vector<std::size_t>&)rows;
- (CGPoint)contentOffsetAt:(double)offset;
- (void)invalidateFrame;

@end

/*
 * hasHeldRow says whether a row is held. UIScrollView's isDragging is the scroll gesture, not
 * this.
 */
@interface ShadowListKitListView (Drag)

- (BOOL)hasHeldRow;
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

/*
 * Data changes. structureChanged runs after every change: sticky rows follow the sections, the
 * selection drops removed rows and a waiting saved position lands.
 */
@interface ShadowListKitListView (Data)

- (void)structureChanged;

@end

/*
 * Sections over the rows. itemForRow: and rowForItem: convert indices, NSNotFound when there is
 * none. updateStickyRows hands the core the pinned rows. reloadSectionIndex reads the index
 * titles again and layoutSectionIndex keeps the index on the trailing edge.
 */
@interface ShadowListKitListView (Sections)

- (NSInteger)itemForRow:(NSInteger)row;
- (NSInteger)rowForItem:(NSInteger)item;
- (BOOL)itemsForDragFromRow:(std::size_t)fromRow toRow:(std::size_t)toRow from:(NSInteger *)from to:(NSInteger *)to;
- (void)updateStickyRows;
- (void)reloadSectionIndex;
- (void)layoutSectionIndex;

@end

/*
 * Selection. userSelectedCell: is a tap on a row. clearSelection deselects every row without
 * delegate calls. unhighlight takes the highlight off the touched row.
 */
@interface ShadowListKitListView (Selection)

- (void)userSelectedCell:(ShadowListKitListCell *)cell;
- (void)clearSelection;
- (void)unhighlight;

@end

/*
 * The saved position and the scroll commands. scrollToRow:viewPosition:animated: scrolls to a
 * row, which can be a section header. stopScrolling stops momentum and an animated command.
 * restorePendingAnchor lands a waiting saved position once its key is there.
 */
@interface ShadowListKitListView (Commands)

- (void)scrollToRow:(std::size_t)row viewPosition:(CGFloat)viewPosition animated:(BOOL)animated;
- (void)stopScrolling;
- (void)restorePendingAnchor;

@end

/*
 * Every row as an accessibility element, and VoiceOver page scrolls. focusAccessibilityRowForKey:
 * scrolls a row a stand-in stood for into view and moves focus to its cell.
 */
@interface ShadowListKitListView (Accessibility)

- (void)focusAccessibilityRowForKey:(NSString *)key;

@end

NS_ASSUME_NONNULL_END
