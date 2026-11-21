#pragma once

#import <UIKit/UIKit.h>

#import <ShadowListKit/ShadowListKitListModels.h>
#import <ShadowListKit/ShadowListKitItemSizeSpec.h>
#import <ShadowListKit/ShadowListKitViewabilityConfig.h>
#import <ShadowListKit/ShadowListKitListProtocols.h>

NS_ASSUME_NONNULL_BEGIN

@class ShadowListKitListView;

/*
 * The view that shows an item. Subclasses lay out their subviews by hand in layoutSubviews and
 * report their height in sizeThatFits:. Without an override, sizeThatFits: fits the cell's Auto
 * Layout constraints to the given width, which makes a cell built with constraints self-sizing.
 * index is the item the cell shows, or NSNotFound while it waits in the reuse pool and for a
 * section header or footer. prepareForReuse is called before the cell is handed out again by
 * dequeueReusableCellWithIdentifier:.
 *
 * The list sets highlighted while a finger rests on a selectable item, selected for selected
 * items and editing while the list is editing. Override the animated setters to show them. They are
 * called only when the value changes.
 */
@interface ShadowListKitListCell : UIView

@property (nonatomic, copy, readonly, nullable) NSString *reuseIdentifier;
@property (nonatomic, readonly) NSInteger index;
@property (nonatomic, getter=isHighlighted) BOOL highlighted;
@property (nonatomic, getter=isSelected) BOOL selected;
@property (nonatomic, getter=isEditing) BOOL editing;

- (instancetype)initWithReuseIdentifier:(nullable NSString *)reuseIdentifier NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;
- (void)prepareForReuse NS_REQUIRES_SUPER;
- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated NS_REQUIRES_SUPER;
- (void)setSelected:(BOOL)selected animated:(BOOL)animated NS_REQUIRES_SUPER;
- (void)setEditing:(BOOL)editing animated:(BOOL)animated NS_REQUIRES_SUPER;

@end

/*
 * A virtualized list on the shadowlist core. Items are placed by the core, which keeps the
 * visible content still while items are measured, inserted or deleted. Measurement, offset
 * corrections and mounting all happen in one layout pass on the main thread. A scroll frame
 * that stays inside the core's offset band does no core work at all.
 *
 * Layout:
 * - inverted: a chat style list that opens at its end and keeps the end in view while the
 *   reader is there. Items stay in data order, oldest first.
 * - followAppends: with inverted, items appended or growing while the reader rests at the end
 *   scroll into view, like an assistant reply. Off by default, which keeps the visible items in
 *   place.
 * - numberOfColumns: columns of a grid. Plain span-one rows keep round-robin placement.
 * - getItemSpan: contiguous columns occupied by an item, clamped to 1...numberOfColumns. The
 *   main-thread callback uses the existing public item index (global across sections). Real
 *   section edges span every column without invoking it; a zero-height edge adds no spacing.
 * - estimatedItemSize: size along the scroll axis assumed for items not measured yet.
 * - overscan: how far past the viewport items are measured, in viewport sizes. Default 1.
 * - mountOverscan: how far past the viewport cells are mounted, in viewport sizes. Default 0.5.
 * - startReachedThreshold, endReachedThreshold: distances to an edge, in viewport sizes, that
 *   fire the reached callbacks. Default 1.
 * - snapToItem, snapAlignment: rest the scroll position on the item edge snapAlignment names.
 * - reorderEnabled: touch and hold an item, then drag it to a new place. Other items slide
 *   aside, and the list scrolls when the item is held near an edge. Works in grids too.
 * - animatesChanges: animate insertItemsAtIndices:, deleteItemsAtIndices: and reloadData.
 *   Inserted items fade in, deleted items fade out and items that stay slide from where they
 *   were to where they are. The visible content stays anchored the same as without animations.
 *   Off by default.
 * - stickyIndices: items that stick to the top of the viewport once scrolled past.
 *   stickySectionHeaders does the same for every section header.
 * - renderStickyHeaderOverlay: an optional custom view for the active sticky row, receiving its
 *   public key, item index and section. A header has NSNotFound as item index; a plain item has
 *   NSNotFound as section. An anonymous empty section has a nil key. Nil restores the normal row.
 * - headerView, footerView: views before the first item and after the last. Their size along
 *   the scroll axis comes from sizeThatFits:. Call setNeedsLayout on the list after one changes
 *   size.
 * - stickyHeader, stickyFooter: pin headerView to the start of the viewport and footerView to
 *   its end, over the items. The footer's place pushes a pinned header off at the end of the
 *   content, and a pinned footer never rides up over the header's place.
 * - emptyView: a view shown while the list has no items, and no section header or footer rows.
 *   It sits right after headerView at its own size, measured like the header's, and footerView
 *   follows it.
 * - nonAnchorKeys: keys of items the list never holds the visible content in place by, like a
 *   typing indicator or an item that streams in. The nearest other item holds it instead.
 * - atEnd: whether an inverted list is at its end. It leaves the end when the reader scrolls
 *   away from it or a scroll command lands away from it, and is back when either brings it
 *   there. Items appended while it rests there do not leave it. The delegate hears
 *   listView:didChangeAtEnd:. A list that is not inverted is always at its end.
 *
 * Data:
 * - reloadData reads the item count and keys again. Items keep their sizes and cells by key,
 *   and the visible content stays in place. Cells of surviving keys are not configured again.
 * - insertItemsAtIndices: and deleteItemsAtIndices: tell the list items were inserted or
 *   deleted, after the data source already reflects it. Only the changed keys are read, and the
 *   rows after the change still shift in the list's arrays. Indices of an insert are
 *   positions in the new data, of a delete positions in the old data. An insert past the end
 *   goes at the end and a delete past the end is dropped.
 * - reloadItemsAtIndices: the items' content changed under the same keys. Visible cells are
 *   configured again and every listed item is measured again. With a payload, the data source's
 *   listView:reconfigureCell:atIndex:payload: can update the shown cell instead.
 * - moveItemAtIndex:toIndex: the item at sourceIndex moved to destinationIndex, after the data
 *   source reflects it.
 * - performBatchUpdates:completion: inserts, deletes, moves and reloads made in the block land
 *   together in one layout and one animation, the way UITableView takes them: deletes, reloads
 *   and move sources are indices in the data before, inserts and move destinations in the data
 *   after. A batch that does not add up reloads everything. completion gets finished once the
 *   change is laid out and, with the default item animator, its animation ended. A custom item
 *   animator's animations are not waited for.
 * - applyChanges reads every key, works out the inserts, deletes and moves against the keys the
 *   list holds, reloads items whose content version changed, and returns what it found.
 * - In a list with sections every change reads the sections and keys again, like reloadData.
 *
 * Pull to refresh: refreshEnabled adds a UIRefreshControl the list owns. refreshing shows it
 * spinning, and the delegate hears listViewDidBeginRefreshing: when the reader pulls.
 *
 * Separators: showsSeparators draws lines between items over the trailing edge of each item's
 * cell. The insets are along the cross axis from its start and end, and the thickness defaults
 * to one device pixel. Prefetching: prefetchDataSource hears which items come next. Animations:
 * itemAnimator animates the changes of animatesChanges.
 *
 * Sections: see the data source. sectionForItemAtIndex:, firstItemIndexInSection:,
 * rectForHeaderInSection:, rectForFooterInSection: and scrollToSection:animated: work in
 * sections. Grid section headers and footers are full-span barriers.
 *
 * Swipe actions and menus come from the delegate. Swiping an item across the scroll axis shows
 * its actions, touching and holding it shows its menu. closeSwipeActionsAnimated: closes an
 * open item. A hold on an item without a menu calls the delegate's
 * listView:didLongPressItemAtIndex:.
 *
 * Selection: a tap selects an item when allowsSelection is set, the default. A tap on a
 * selected item of a list with allowsMultipleSelection deselects it. selectItemAtIndex:animated:
 * and deselectItemAtIndex:animated: change it without delegate calls. Selection follows keys.
 * editing is passed to the cells and turns swipe actions off.
 *
 * Saving the position: anchorState is the item at the viewport start by key, and
 * restoreAnchorState: lands that item the same distance in again, now or once a reload brings
 * its key. The list saves it with UIKit state restoration when it has a restorationIdentifier.
 *
 * Geometry:
 * - visibleRange: the items overlapping the viewport, low to high.
 * - rectForItemAtIndex: the item's frame in content coordinates: from the start of the content,
 *   headerView included, without the content insets or the scroll offset. Estimated when the
 *   item was not measured yet. The section rects use the same space.
 * - scrollToItemAtIndex:viewPosition:animated: brings an item into view. viewPosition is where
 *   it rests in the viewport, from 0 at the start to 1 at the end. The core keeps correcting
 *   until the item lands, even across estimates.
 */
@interface ShadowListKitListView : UIScrollView

@property (nonatomic, weak, nullable) id<ShadowListKitListViewDataSource> dataSource;
@property (nonatomic, weak, nullable) id<ShadowListKitListViewDelegate> delegate;

@property (nonatomic, getter=isInverted) BOOL inverted;
@property (nonatomic) BOOL followAppends;
@property (nonatomic, getter=isHorizontal) BOOL horizontal;
@property (nonatomic) NSInteger numberOfColumns;
@property (nonatomic) CGFloat estimatedItemSize;
@property (nonatomic, copy, nullable) ShadowListKitItemSizeSpec * _Nullable (^getItemSizeSpec)(NSInteger index);
@property (nonatomic) NSUInteger measureLookaheadRows;
@property (nonatomic, copy, nullable) NSInteger (^getItemSpan)(NSInteger index);
@property (nonatomic, copy, nullable) ShadowListKitViewabilityConfig *viewabilityConfig;
@property (nonatomic, copy, nullable) NSArray<ShadowListKitViewabilityConfigCallbackPair *> *viewabilityConfigCallbackPairs;
@property (nonatomic, copy, nullable) void (^onViewableItemsChanged)(ShadowListKitViewableItemsChangedInfo *info);
- (void)recordInteraction;
@property (nonatomic) CGFloat overscan;
@property (nonatomic) CGFloat mountOverscan;
@property (nonatomic) CGFloat startReachedThreshold;
@property (nonatomic) CGFloat endReachedThreshold;
@property (nonatomic) BOOL snapToItem;
@property (nonatomic) ShadowListKitSnapAlignment snapAlignment;
@property (nonatomic) BOOL reorderEnabled;
@property (nonatomic, copy, nullable) NSString *dragBoardId;
@property (nonatomic, copy, nullable) NSString *dragListId;
@property (nonatomic, copy, nullable) NSString *dragListLabel;
@property (nonatomic) BOOL animatesChanges;
@property (nonatomic, copy, nullable) NSIndexSet *stickyIndices;
@property (nonatomic) BOOL stickySectionHeaders;
@property (nonatomic, copy, nullable) UIView * _Nullable (^renderStickyHeaderOverlay)(NSString * _Nullable key, NSInteger index, NSInteger section);
@property (nonatomic, strong, nullable) UIView *headerView;
@property (nonatomic, strong, nullable) UIView *footerView;
@property (nonatomic) BOOL stickyHeader;
@property (nonatomic) BOOL stickyFooter;
/*
 * User scrolls hide these pinned views; commands and corrections retain their hidden amount.
 */
@property (nonatomic) BOOL autoHideHeader;
@property (nonatomic) BOOL autoHideFooter;
@property (nonatomic, strong, nullable) UIView *emptyView;
@property (nonatomic, copy, nullable) NSSet<NSString *> *nonAnchorKeys;
@property (nonatomic, copy, nullable) NSSet<NSString *> *persistentKeys;
@property (nonatomic, readonly, getter=isAtEnd) BOOL atEnd;

@property (nonatomic, weak, nullable) id<ShadowListKitListViewPrefetchDataSource> prefetchDataSource;
@property (nonatomic, strong, null_resettable) id<ShadowListKitItemAnimator> itemAnimator;

@property (nonatomic) BOOL allowsSelection;
@property (nonatomic) BOOL allowsMultipleSelection;
@property (nonatomic, getter=isEditing) BOOL editing;
@property (nonatomic, readonly) NSIndexSet *selectedIndices;

@property (nonatomic) BOOL refreshEnabled;
@property (nonatomic, getter=isRefreshing) BOOL refreshing;

@property (nonatomic) BOOL showsSeparators;
@property (nonatomic, strong, null_resettable) UIColor *separatorColor;
@property (nonatomic) CGFloat separatorInsetStart;
@property (nonatomic) CGFloat separatorInsetEnd;
@property (nonatomic) CGFloat separatorThickness;

@property (nonatomic, readonly) NSInteger numberOfSections;
@property (nonatomic, readonly, nullable) ShadowListKitAnchorState *anchorState;

- (void)registerClass:(Class)cellClass forCellReuseIdentifier:(NSString *)identifier;
- (__kindof ShadowListKitListCell *)dequeueReusableCellWithIdentifier:(NSString *)identifier;

- (void)reloadData;
- (void)insertItemsAtIndices:(NSIndexSet *)indices;
- (void)deleteItemsAtIndices:(NSIndexSet *)indices;
- (void)reloadItemsAtIndices:(NSIndexSet *)indices;
- (void)reloadItemsAtIndices:(NSIndexSet *)indices payload:(nullable id)payload;
- (void)moveItemAtIndex:(NSInteger)sourceIndex toIndex:(NSInteger)destinationIndex;
- (void)performBatchUpdates:(void (NS_NOESCAPE ^_Nullable)(void))updates completion:(void (^_Nullable)(BOOL finished))completion;
- (ShadowListKitListChanges *)applyChanges;

- (void)setEditing:(BOOL)editing animated:(BOOL)animated;
- (void)selectItemAtIndex:(NSInteger)index animated:(BOOL)animated;
- (void)deselectItemAtIndex:(NSInteger)index animated:(BOOL)animated;
- (void)closeSwipeActionsAnimated:(BOOL)animated;
- (void)restoreAnchorState:(ShadowListKitAnchorState *)state NS_SWIFT_NAME(restoreAnchorState(_:));

- (NSInteger)sectionForItemAtIndex:(NSInteger)index;
- (NSInteger)firstItemIndexInSection:(NSInteger)section NS_SWIFT_NAME(firstItemIndexInSection(_:));
- (CGRect)rectForHeaderInSection:(NSInteger)section;
- (CGRect)rectForFooterInSection:(NSInteger)section;
- (void)scrollToSection:(NSInteger)section animated:(BOOL)animated NS_SWIFT_NAME(scrollToSection(_:animated:));

- (nullable __kindof ShadowListKitListCell *)cellForItemAtIndex:(NSInteger)index;
@property (nonatomic, readonly) NSArray<__kindof ShadowListKitListCell *> *visibleCells;
@property (nonatomic, readonly) NSRange visibleRange;
- (CGRect)rectForItemAtIndex:(NSInteger)index;

- (void)scrollToItemAtIndex:(NSInteger)index viewPosition:(CGFloat)viewPosition animated:(BOOL)animated;
- (void)scrollToStartAnimated:(BOOL)animated;
- (void)scrollToEndAnimated:(BOOL)animated;

@end

NS_ASSUME_NONNULL_END
