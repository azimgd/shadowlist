#import <ShadowListKit/ShadowListKitListView.h>

#include <string>
#include <unordered_map>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/SwipeReveal.hpp>

#import "ShadowListKitListCell+Private.h"

NS_ASSUME_NONNULL_BEGIN

@class ShadowListKitSwipeActionsView;

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
 * for item cells only. itemForRow: and rowForItem: convert indices, NSNotFound when there is none.
 */
@interface ShadowListKitListView () <UIScrollViewDelegate> {
 @package
  azimgd::shadowlist::ListDriver _driver;
  azimgd::shadowlist::ListSections _sections;
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
- (NSInteger)itemForRow:(NSInteger)row;
- (NSInteger)rowForItem:(NSInteger)item;
- (BOOL)itemsForDragFromRow:(std::size_t)fromRow toRow:(std::size_t)toRow from:(NSInteger *)from to:(NSInteger *)to;

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
 * swipeCellWillRecycle: drops the swipe of a cell going back to the pool. showMenuForCell:atPoint: shows a held row's menu.
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

NS_ASSUME_NONNULL_END
