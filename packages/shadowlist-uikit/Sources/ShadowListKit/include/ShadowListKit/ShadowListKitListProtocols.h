#pragma once

#import <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#import <ShadowListKit/ShadowListKitListModels.h>

/*
 * The menu class of the platform the kit builds for: UIKit's on iOS, AppKit's on macOS.
 */
#if TARGET_OS_OSX
@compatibility_alias ShadowListKitPlatformMenu NSMenu;
#else
@compatibility_alias ShadowListKitPlatformMenu UIMenu;
#endif

NS_ASSUME_NONNULL_BEGIN

@class ShadowListKitListView;
@class ShadowListKitListCell;

/*
 * Which edge of an item a snapped scroll position lines up with the viewport. The order
 * matches the core's SnapAlignment.
 */
typedef NS_ENUM(NSInteger, ShadowListKitSnapAlignment) {
  ShadowListKitSnapAlignmentStart,
  ShadowListKitSnapAlignmentCenter,
  ShadowListKitSnapAlignmentEnd,
};

/*
 * Animates items for animatesChanges. The list works out which items were inserted, deleted and
 * moved, places them and calls these to animate. animateDeleteOfCell: must call completion when
 * it ends, which gives the cell back to the reuse pool. animateMoveOfCell: gets a cell already at
 * its new place, and offset is where it showed before, relative to that place.
 */
NS_SWIFT_UI_ACTOR
@protocol ShadowListKitItemAnimator <NSObject>

- (void)listView:(ShadowListKitListView *)listView animateInsertOfCell:(ShadowListKitListCell *)cell NS_SWIFT_NAME(listView(_:animateInsertOf:));
- (void)listView:(ShadowListKitListView *)listView animateDeleteOfCell:(ShadowListKitListCell *)cell completion:(void (^)(void))completion NS_SWIFT_NAME(listView(_:animateDeleteOf:completion:));
- (void)listView:(ShadowListKitListView *)listView animateMoveOfCell:(ShadowListKitListCell *)cell fromOffset:(CGPoint)offset NS_SWIFT_NAME(listView(_:animateMoveOf:fromOffset:));

@end

/*
 * The animator a list starts with: inserted items fade in, deleted items fade out and items
 * that stay slide, for duration seconds.
 */
@interface ShadowListKitDefaultItemAnimator : NSObject <ShadowListKitItemAnimator>

@property (nonatomic) NSTimeInterval duration;

@end

/*
 * Hears which items the list will soon show, to load what their cells need ahead. Prefetched
 * items are the ones in the core's measured range, an overscan past the viewport, outside the
 * mounted ones. The layout pass that brings an item into the range measures the items it will
 * show first, then prefetches the rest before it measures them. Without
 * listView:sizeForItemAtIndex:crossSize: that is before listView:cellForItemAtIndex: makes their
 * cells. A prefetched item that leaves that range before it shows is cancelled.
 */
NS_SWIFT_UI_ACTOR
@protocol ShadowListKitListViewPrefetchDataSource <NSObject>

- (void)listView:(ShadowListKitListView *)listView prefetchItemsAtIndices:(NSIndexSet *)indices NS_SWIFT_NAME(listView(_:prefetchItemsAt:));

@optional

- (void)listView:(ShadowListKitListView *)listView cancelPrefetchingForItemsAtIndices:(NSIndexSet *)indices NS_SWIFT_NAME(listView(_:cancelPrefetchingForItemsAt:));

@end

NS_SWIFT_UI_ACTOR
@protocol ShadowListKitListViewDataSource <NSObject>

- (NSInteger)numberOfItemsInListView:(ShadowListKitListView *)listView;

/*
 * A stable identity for the item. Sizes, cells and the scroll position follow keys across
 * reloadData. A prepend or an insert above keeps what is on screen in place.
 * An item whose content changed under the same key needs reloadItemsAtIndices:.
 */
- (NSString *)listView:(ShadowListKitListView *)listView keyForItemAtIndex:(NSInteger)index;

- (ShadowListKitListCell *)listView:(ShadowListKitListView *)listView cellForItemAtIndex:(NSInteger)index;

@optional

/*
 * The item's size along the scroll axis for the given cross size. Implement it when sizes are
 * known without a view, like from a layout precomputed off the main thread. Otherwise every
 * item is measured once through a sizing cell's sizeThatFits:.
 */
- (CGFloat)listView:(ShadowListKitListView *)listView sizeForItemAtIndex:(NSInteger)index crossSize:(CGFloat)crossSize;

/*
 * Sections. A data source with numberOfSectionsInListView: groups its items in sections and
 * gives their counts with listView:numberOfItemsInSection:. numberOfItemsInListView: is then
 * not read. Item indices still run across all sections, the same as without sections.
 * A section has a header when listView:titleForHeaderInSection: returns a title, an empty one
 * too, and a footer the same way. The list shows the title in a plain cell unless
 * listView:cellForHeaderInSection: gives one. Sizes come from listView:sizeForHeaderInSection:
 * when implemented, otherwise the cell is measured. listView:keyForSection: gives the header
 * and footer their identity, by default the key of the section's first item.
 */
- (NSInteger)numberOfSectionsInListView:(ShadowListKitListView *)listView;
- (NSInteger)listView:(ShadowListKitListView *)listView numberOfItemsInSection:(NSInteger)section;
- (NSString *)listView:(ShadowListKitListView *)listView keyForSection:(NSInteger)section;
- (nullable NSString *)listView:(ShadowListKitListView *)listView titleForHeaderInSection:(NSInteger)section;
- (nullable NSString *)listView:(ShadowListKitListView *)listView titleForFooterInSection:(NSInteger)section;
- (ShadowListKitListCell *)listView:(ShadowListKitListView *)listView cellForHeaderInSection:(NSInteger)section;
- (ShadowListKitListCell *)listView:(ShadowListKitListView *)listView cellForFooterInSection:(NSInteger)section;
- (CGFloat)listView:(ShadowListKitListView *)listView sizeForHeaderInSection:(NSInteger)section crossSize:(CGFloat)crossSize;
- (CGFloat)listView:(ShadowListKitListView *)listView sizeForFooterInSection:(NSInteger)section crossSize:(CGFloat)crossSize;

#if !TARGET_OS_OSX
/*
 * Titles of the section index along the trailing edge, and the section a title scrolls to,
 * by default the section at the title's position. iOS only.
 */
- (nullable NSArray<NSString *> *)sectionIndexTitlesForListView:(ShadowListKitListView *)listView NS_SWIFT_NAME(sectionIndexTitles(for:));
- (NSInteger)listView:(ShadowListKitListView *)listView sectionForSectionIndexTitle:(NSString *)title atIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:sectionForSectionIndexTitle:at:));
#endif

/*
 * Update a shown cell for a payload given to reloadItemsAtIndices:payload: without a new cell.
 * Return NO to have the item reloaded in full instead.
 */
- (BOOL)listView:(ShadowListKitListView *)listView
  reconfigureCell:(ShadowListKitListCell *)cell
          atIndex:(NSInteger)index
          payload:(nullable id)payload
    NS_SWIFT_NAME(listView(_:reconfigureCell:at:payload:));

/*
 * A value that changes when the item's content changes under the same key, like a revision or
 * a hash. applyChanges reloads the items whose value changed.
 */
- (NSInteger)listView:(ShadowListKitListView *)listView contentVersionForItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:contentVersionForItemAt:));

@end

/*
 * On iOS the delegate inherits UIScrollViewDelegate only because the list is a UIScrollView. The
 * scroll view's calls still arrive, and the list's own scroll events below are the same on both
 * platforms.
 */
NS_SWIFT_UI_ACTOR
#if TARGET_OS_OSX
@protocol ShadowListKitListViewDelegate <NSObject>
#else
@protocol ShadowListKitListViewDelegate <UIScrollViewDelegate>
#endif

@optional

/*
 * A cell that fades out after a delete ends displaying when its fade ends, with the index its
 * item had before the delete, the way UITableView reports a deleted row.
 */
- (void)listView:(ShadowListKitListView *)listView willDisplayCell:(ShadowListKitListCell *)cell atIndex:(NSInteger)index;
- (void)listView:(ShadowListKitListView *)listView didEndDisplayingCell:(ShadowListKitListCell *)cell atIndex:(NSInteger)index;
- (void)listView:(ShadowListKitListView *)listView didSelectItemAtIndex:(NSInteger)index;
- (void)listView:(ShadowListKitListView *)listView didDeselectItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:didDeselectItemAt:));
- (BOOL)listView:(ShadowListKitListView *)listView shouldSelectItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:shouldSelectItemAt:));
- (BOOL)listView:(ShadowListKitListView *)listView shouldHighlightItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:shouldHighlightItemAt:));

#if TARGET_OS_OSX
/*
 * The reader opened an item: a double-click on it, or Return or Space on the selection. The
 * item is the one the keyboard is on when it is selected, otherwise the first selected one.
 * macOS only.
 */
- (void)listView:(ShadowListKitListView *)listView didActivateItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:didActivateItemAt:));

/*
 * The text typing selects an item by, or nil for an item typing skips. Typed characters within
 * a second of each other build one prefix, matched without case or diacritics from the item the
 * keyboard is on. It is read for any item, shown or not. macOS only.
 */
- (nullable NSString *)listView:(ShadowListKitListView *)listView typeSelectStringForItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:typeSelectStringForItemAt:));
#endif

/*
 * Actions behind an item swiped from its leading or trailing side, or nil for none.
 */
- (nullable ShadowListKitSwipeActionsConfiguration *)listView:(ShadowListKitListView *)listView
                  leadingSwipeActionsForItemAtIndex:(NSInteger)index
    NS_SWIFT_NAME(listView(_:leadingSwipeActionsForItemAt:));
- (nullable ShadowListKitSwipeActionsConfiguration *)listView:(ShadowListKitListView *)listView
                 trailingSwipeActionsForItemAtIndex:(NSInteger)index
    NS_SWIFT_NAME(listView(_:trailingSwipeActionsForItemAt:));

/*
 * The menu for an item, or nil for none. On iOS it shows on touching and holding the item. An
 * item that can also be reordered lifts on the hold, and letting go without moving it shows the
 * menu. On macOS it shows on a right click or a Control-click on the item, or the menu key on
 * the keyboard's item. With none, an item with swipe actions shows them as a menu, and
 * clickedIndex is the item while the menu is open.
 */
- (nullable ShadowListKitPlatformMenu *)listView:(ShadowListKitListView *)listView contextMenuForItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:contextMenuForItemAt:));

/*
 * Whether the separator below an item shows when showsSeparators is set. Separators only go
 * between items of one section.
 */
- (BOOL)listView:(ShadowListKitListView *)listView showsSeparatorAfterItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:showsSeparatorAfterItemAt:));

#if !TARGET_OS_OSX
/*
 * The reader pulled to refresh with refreshEnabled. Set refreshing to NO when done. iOS only.
 */
- (void)listViewDidBeginRefreshing:(ShadowListKitListView *)listView;
#endif

/*
 * Whether an item can be picked up when reorderEnabled is set. Every item can by default.
 */
- (BOOL)listView:(ShadowListKitListView *)listView canMoveItemAtIndex:(NSInteger)index;

/*
 * A held item was dropped at another index. Move the item in the data. The list reads the
 * data again right after and keeps the dropped item where it was let go.
 */
- (void)listView:(ShadowListKitListView *)listView moveItemAtIndex:(NSInteger)sourceIndex toIndex:(NSInteger)destinationIndex;

/*
 * The scroll position came within startReachedThreshold or endReachedThreshold of an edge.
 */
- (void)listViewDidReachStart:(ShadowListKitListView *)listView;
- (void)listViewDidReachEnd:(ShadowListKitListView *)listView;

/*
 * The scroll events. listViewDidScroll: follows every change of contentOffset. The others follow
 * the reader's dragging and the deceleration after it: on iOS the scroll view's, on macOS
 * scrollPhase's, where dragging begins and ends and the settling after it ends.
 * listViewDidEndScrollingAnimation: follows an animated scroll command.
 */
- (void)listViewDidScroll:(ShadowListKitListView *)listView;
- (void)listViewWillBeginDragging:(ShadowListKitListView *)listView;
- (void)listViewDidEndDragging:(ShadowListKitListView *)listView willDecelerate:(BOOL)decelerate;
- (void)listViewDidEndDecelerating:(ShadowListKitListView *)listView;
- (void)listViewDidEndScrollingAnimation:(ShadowListKitListView *)listView;

#if TARGET_OS_OSX
/*
 * A layout pass ended. The cells of the rows near the viewport are mounted and placed. macOS
 * only.
 */
- (void)listViewDidLayOut:(ShadowListKitListView *)listView;

/*
 * With scrollDriven, the content offset the list wants: a correction that keeps the visible
 * items still, the target of a scroll command or the edge scroll of a held row. Set
 * contentOffset to it inside this call. The list lays out at the offset it has when the call
 * returns. macOS only.
 */
- (void)listView:(ShadowListKitListView *)listView didCorrectOffset:(CGPoint)contentOffset;
#endif

@end

NS_ASSUME_NONNULL_END
