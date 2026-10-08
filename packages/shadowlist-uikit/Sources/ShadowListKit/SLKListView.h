#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@class SLKListView;

/*
 * A row view. Subclasses lay out their subviews by hand in layoutSubviews and report their
 * height in sizeThatFits:. Without an override, sizeThatFits: fits the cell's Auto Layout
 * constraints to the given width, which makes a cell built with constraints self-sizing. index is
 * the item the cell shows, or NSNotFound while it waits in the reuse pool and for a section
 * header or footer. prepareForReuse is called before the cell is handed out again by
 * dequeueReusableCellWithIdentifier:.
 *
 * The list sets highlighted while a finger rests on a selectable row, selected for selected rows
 * and editing while the list is editing. Override the animated setters to show them. They are
 * called only when the value changes.
 */
@interface SLKListCell : UIView

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

typedef NS_ENUM(NSInteger, SLKSwipeActionStyle) {
  SLKSwipeActionStyleNormal,
  SLKSwipeActionStyleDestructive,
};

/*
 * A button shown behind a swiped row. The handler runs when the button is tapped or the row is
 * swiped all the way. Call completion with whether the action was performed. The row then
 * closes, unless the action removed it.
 */
@interface SLKSwipeAction : NSObject

+ (instancetype)actionWithStyle:(SLKSwipeActionStyle)style
                          title:(nullable NSString *)title
                        handler:(void (^)(SLKSwipeAction *action, void (^completion)(BOOL performed)))handler;

@property (nonatomic, readonly) SLKSwipeActionStyle style;
@property (nonatomic, copy, nullable) NSString *title;
@property (nonatomic, strong, nullable) UIImage *image;
@property (nonatomic, strong, null_resettable) UIColor *backgroundColor;
@property (nonatomic, copy, readonly) void (^handler)(SLKSwipeAction *action, void (^completion)(BOOL performed));

@end

/*
 * The actions of one side of a row. The first action is nearest the edge. With
 * performsFirstActionWithFullSwipe, the default, swiping the row all the way performs it, like
 * swipe to dismiss.
 */
@interface SLKSwipeActionsConfiguration : NSObject

+ (instancetype)configurationWithActions:(NSArray<SLKSwipeAction *> *)actions;

@property (nonatomic, copy, readonly) NSArray<SLKSwipeAction *> *actions;
@property (nonatomic) BOOL performsFirstActionWithFullSwipe;

@end

/*
 * A scroll position that survives data changes: the key of the row at the viewport start and
 * how far the viewport start is past that row's leading edge.
 */
@interface SLKAnchorState : NSObject <NSSecureCoding>

- (instancetype)initWithKey:(NSString *)key offset:(CGFloat)offset NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property (nonatomic, copy, readonly) NSString *key;
@property (nonatomic, readonly) CGFloat offset;

@end

/*
 * What applyChanges found. Deleted and moved from are indices in the previous data, inserted,
 * moved to and reloaded indices in the new data.
 */
@interface SLKListChanges : NSObject

@property (nonatomic, copy, readonly) NSIndexSet *deletedIndices;
@property (nonatomic, copy, readonly) NSIndexSet *insertedIndices;
@property (nonatomic, copy, readonly) NSArray<NSNumber *> *movedFromIndices;
@property (nonatomic, copy, readonly) NSArray<NSNumber *> *movedToIndices;
@property (nonatomic, copy, readonly) NSIndexSet *reloadedIndices;
@property (nonatomic, readonly, getter=isEmpty) BOOL empty;

@end

/*
 * Animates rows for animatesChanges. The list works out which rows came, went and moved, places
 * them and calls these to animate. animateRemovalOfCell: must call completion when it ends, which
 * gives the cell back to the reuse pool. animateMoveOfCell: gets a cell already at its new place,
 * and offset is where it showed before, relative to that place.
 */
@protocol SLKItemAnimator <NSObject>

- (void)listView:(SLKListView *)listView animateInsertOfCell:(SLKListCell *)cell NS_SWIFT_NAME(listView(_:animateInsertOf:));
- (void)listView:(SLKListView *)listView animateRemovalOfCell:(SLKListCell *)cell completion:(void (^)(void))completion NS_SWIFT_NAME(listView(_:animateRemovalOf:completion:));
- (void)listView:(SLKListView *)listView animateMoveOfCell:(SLKListCell *)cell fromOffset:(CGPoint)offset NS_SWIFT_NAME(listView(_:animateMoveOf:fromOffset:));

@end

/*
 * The animator a list starts with: new rows fade in, removed rows fade out and rows that stay
 * slide, for duration seconds.
 */
@interface SLKDefaultItemAnimator : NSObject <SLKItemAnimator>

@property (nonatomic) NSTimeInterval duration;

@end

/*
 * Hears which items the list will soon show, to load what their cells need ahead. Prefetched
 * items are the ones in the core's measured window, an overscan past the viewport, that have no
 * cell yet. A prefetched item that leaves the window before it shows is cancelled.
 */
@protocol SLKListViewPrefetchDataSource <NSObject>

- (void)listView:(SLKListView *)listView prefetchItemsAtIndices:(NSIndexSet *)indices NS_SWIFT_NAME(listView(_:prefetchItemsAt:));

@optional

- (void)listView:(SLKListView *)listView cancelPrefetchingForItemsAtIndices:(NSIndexSet *)indices NS_SWIFT_NAME(listView(_:cancelPrefetchingForItemsAt:));

@end

@protocol SLKListViewDataSource <NSObject>

- (NSInteger)numberOfItemsInListView:(SLKListView *)listView;

/*
 * A stable identity for the row. Sizes, cells and the scroll position follow keys across
 * reloadData. A prepend or an insert above keeps what is on screen in place.
 * A row whose content changed under the same key needs reloadItemsAtIndices:.
 */
- (NSString *)listView:(SLKListView *)listView keyForItemAtIndex:(NSInteger)index;

- (SLKListCell *)listView:(SLKListView *)listView cellForItemAtIndex:(NSInteger)index;

@optional

/*
 * The row's size along the scroll axis for the given cross size. Implement it when sizes are
 * known without a view, like from a layout precomputed off the main thread. Otherwise every
 * row is measured once through a sizing cell's sizeThatFits:.
 */
- (CGFloat)listView:(SLKListView *)listView sizeForItemAtIndex:(NSInteger)index crossSize:(CGFloat)crossSize;

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
- (NSInteger)numberOfSectionsInListView:(SLKListView *)listView;
- (NSInteger)listView:(SLKListView *)listView numberOfItemsInSection:(NSInteger)section;
- (NSString *)listView:(SLKListView *)listView keyForSection:(NSInteger)section;
- (nullable NSString *)listView:(SLKListView *)listView titleForHeaderInSection:(NSInteger)section;
- (nullable NSString *)listView:(SLKListView *)listView titleForFooterInSection:(NSInteger)section;
- (SLKListCell *)listView:(SLKListView *)listView cellForHeaderInSection:(NSInteger)section;
- (SLKListCell *)listView:(SLKListView *)listView cellForFooterInSection:(NSInteger)section;
- (CGFloat)listView:(SLKListView *)listView sizeForHeaderInSection:(NSInteger)section crossSize:(CGFloat)crossSize;
- (CGFloat)listView:(SLKListView *)listView sizeForFooterInSection:(NSInteger)section crossSize:(CGFloat)crossSize;

/*
 * Titles of the section index along the trailing edge, and the section a title scrolls to,
 * by default the section at the title's position.
 */
- (nullable NSArray<NSString *> *)sectionIndexTitlesForListView:(SLKListView *)listView NS_SWIFT_NAME(sectionIndexTitles(for:));
- (NSInteger)listView:(SLKListView *)listView sectionForSectionIndexTitle:(NSString *)title atIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:sectionForSectionIndexTitle:at:));

/*
 * Update a shown cell for a payload given to reloadItemsAtIndices:payload: without a new cell.
 * Return NO to have the row reloaded in full instead.
 */
- (BOOL)listView:(SLKListView *)listView
  reconfigureCell:(SLKListCell *)cell
          atIndex:(NSInteger)index
          payload:(nullable id)payload
    NS_SWIFT_NAME(listView(_:reconfigureCell:at:payload:));

/*
 * A value that changes when the item's content changes under the same key, like a revision or
 * a hash. applyChanges reloads the rows whose value changed.
 */
- (NSInteger)listView:(SLKListView *)listView contentVersionForItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:contentVersionForItemAt:));

@end

@protocol SLKListViewDelegate <UIScrollViewDelegate>

@optional

- (void)listView:(SLKListView *)listView willDisplayCell:(SLKListCell *)cell atIndex:(NSInteger)index;
- (void)listView:(SLKListView *)listView didEndDisplayingCell:(SLKListCell *)cell atIndex:(NSInteger)index;
- (void)listView:(SLKListView *)listView didSelectItemAtIndex:(NSInteger)index;
- (void)listView:(SLKListView *)listView didDeselectItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:didDeselectItemAt:));
- (BOOL)listView:(SLKListView *)listView shouldSelectItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:shouldSelectItemAt:));
- (BOOL)listView:(SLKListView *)listView shouldHighlightItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:shouldHighlightItemAt:));

/*
 * Actions behind a row swiped from its leading or trailing side, or nil for none.
 */
- (nullable SLKSwipeActionsConfiguration *)listView:(SLKListView *)listView
                  leadingSwipeActionsForItemAtIndex:(NSInteger)index
    NS_SWIFT_NAME(listView(_:leadingSwipeActionsForItemAt:));
- (nullable SLKSwipeActionsConfiguration *)listView:(SLKListView *)listView
                 trailingSwipeActionsForItemAtIndex:(NSInteger)index
    NS_SWIFT_NAME(listView(_:trailingSwipeActionsForItemAt:));

/*
 * The menu for touching and holding a row, or nil for none. A row that can also be reordered
 * lifts on the hold. Letting go without moving it shows the menu.
 */
- (nullable UIMenu *)listView:(SLKListView *)listView contextMenuForItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:contextMenuForItemAt:));

/*
 * Whether the separator below an item shows when showsSeparators is set. Separators only go
 * between items of one section.
 */
- (BOOL)listView:(SLKListView *)listView showsSeparatorAfterItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:showsSeparatorAfterItemAt:));

/*
 * The reader pulled to refresh with refreshEnabled. Set refreshing to NO when done.
 */
- (void)listViewDidBeginRefreshing:(SLKListView *)listView;

/*
 * Whether a row can be picked up when reorderEnabled is set. Every row can by default.
 */
- (BOOL)listView:(SLKListView *)listView canMoveItemAtIndex:(NSInteger)index;

/*
 * A held row was dropped at another index. Move the item in the data, the list reads the
 * data again right after and keeps the dropped row where it was let go.
 */
- (void)listView:(SLKListView *)listView moveItemAtIndex:(NSInteger)sourceIndex toIndex:(NSInteger)destinationIndex;

/*
 * The scroll position came within startReachedThreshold or endReachedThreshold of an edge.
 */
- (void)listViewDidReachStart:(SLKListView *)listView;
- (void)listViewDidReachEnd:(SLKListView *)listView;

@end

/*
 * A virtualized list on the shadowlist core. Rows are placed by the core, which keeps the
 * visible content still while rows are measured, inserted or removed. Measurement, offset
 * corrections and mounting all happen in one layout pass on the main thread. A scroll frame
 * that stays inside the core's offset band does no core work at all.
 *
 * Layout:
 * - inverted: a chat style list that opens at its end and keeps the end in view while the
 *   reader is there. Rows stay in data order, oldest first.
 * - followAppends: with inverted, rows appended while the reader rests at the end scroll into
 *   view, like an assistant reply. Off by default, which keeps the visible rows in place.
 * - numberOfColumns: columns of a grid. Row i goes into column i % numberOfColumns and each
 *   column stacks its own rows.
 * - estimatedItemSize: size along the scroll axis assumed for rows not measured yet.
 * - overscan: how far past the viewport rows are measured, in viewport sizes. Default 1.
 * - mountOverscan: how far past the viewport cells are mounted, in viewport sizes. Default 0.5.
 * - startReachedThreshold, endReachedThreshold: distances to an edge, in viewport sizes, that
 *   fire the reached callbacks. Default 1.
 * - snapToItem, snapAlignment: rest the scroll position on a row edge. Alignment 0 start,
 *   1 center, 2 end.
 * - reorderEnabled: touch and hold a row, then drag it to a new place. Other rows slide aside,
 *   and the list scrolls when the row is held near an edge. Works in grids too.
 * - animatesChanges: animate insertItemsAtIndices:, deleteItemsAtIndices: and reloadData. New
 *   rows fade in, removed rows fade out and rows that stay slide from where they were to where
 *   they are. The visible content stays anchored the same as without animations. Off by
 *   default.
 * - stickyIndices: items that stick to the top of the viewport once scrolled past.
 *   stickySectionHeaders does the same for every section header.
 * - headerView, footerView: views before the first row and after the last. Their size along
 *   the scroll axis comes from sizeThatFits:. Call setNeedsLayout on the list after one changes
 *   size.
 *
 * Data:
 * - reloadData reads the row count and keys again. Rows keep their sizes and cells by key, and
 *   the visible content stays in place. Cells of surviving keys are not configured again.
 * - insertItemsAtIndices: and deleteItemsAtIndices: tell the list rows were inserted or
 *   deleted, after the data source already reflects it. Only the changed keys are read. A
 *   prepend to a long list costs the same as to a short one. Indices of an insert are
 *   positions in the new data, of a delete positions in the old data.
 * - reloadItemsAtIndices: the rows' content changed under the same keys. Visible cells are
 *   configured again and every listed row is measured again. With a payload, the data source's
 *   listView:reconfigureCell:atIndex:payload: can update the shown cell instead.
 * - moveItemAtIndex:toIndex: an item moved, after the data source reflects it.
 * - performBatchUpdates:completion: inserts, deletes, moves and reloads made in the block land
 *   together in one layout and one animation, the way UITableView takes them: deletes, reloads
 *   and move sources are indices in the data before, inserts and move destinations in the data
 *   after. A batch that does not add up reloads everything.
 * - applyChanges reads every key, works out the inserts, deletes and moves against the keys the
 *   list holds, reloads rows whose content version changed, and returns what it found.
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
 * rectForHeaderInSection: and scrollToSection:animated: work in sections. A grid places section
 * headers in a column slot like any row, full width rows are not supported.
 *
 * Swipe actions and menus come from the delegate. Swiping a row across the scroll axis shows its
 * actions, touching and holding it shows its menu. closeSwipeActionsAnimated: closes an open row.
 *
 * Selection: a tap selects a row when allowsSelection is set, the default. A tap on a selected
 * row of a list with allowsMultipleSelection deselects it. selectItemAtIndex:animated: and
 * deselectItemAtIndex:animated: change it without delegate calls. Selection follows keys.
 * editing is passed to the cells and turns swipe actions off.
 *
 * Saving the position: anchorState is the row at the viewport start by key, and
 * restoreAnchorState: lands that row the same distance in again, now or once a reload brings
 * its key. The list saves it with UIKit state restoration when it has a restorationIdentifier.
 *
 * Geometry:
 * - visibleRange: the rows overlapping the viewport, low to high.
 * - rectForItemAtIndex: the row's frame in content coordinates, estimated when it was not
 *   measured yet.
 * - scrollToItemAtIndex:viewPosition:animated: brings a row into view. viewPosition is where it
 *   rests in the viewport, from 0 at the start to 1 at the end. The core keeps correcting until
 *   the row lands, even across estimates.
 */
@interface SLKListView : UIScrollView

@property (nonatomic, weak, nullable) id<SLKListViewDataSource> dataSource;
@property (nonatomic, weak, nullable) id<SLKListViewDelegate> delegate;

@property (nonatomic, getter=isInverted) BOOL inverted;
@property (nonatomic) BOOL followAppends;
@property (nonatomic, getter=isHorizontal) BOOL horizontal;
@property (nonatomic) NSInteger numberOfColumns;
@property (nonatomic) CGFloat estimatedItemSize;
@property (nonatomic) CGFloat overscan;
@property (nonatomic) CGFloat mountOverscan;
@property (nonatomic) CGFloat startReachedThreshold;
@property (nonatomic) CGFloat endReachedThreshold;
@property (nonatomic) BOOL snapToItem;
@property (nonatomic) NSInteger snapAlignment;
@property (nonatomic) BOOL reorderEnabled;
@property (nonatomic) BOOL animatesChanges;
@property (nonatomic, copy, nullable) NSIndexSet *stickyIndices;
@property (nonatomic) BOOL stickySectionHeaders;
@property (nonatomic, strong, nullable) UIView *headerView;
@property (nonatomic, strong, nullable) UIView *footerView;

@property (nonatomic, weak, nullable) id<SLKListViewPrefetchDataSource> prefetchDataSource;
@property (nonatomic, strong, null_resettable) id<SLKItemAnimator> itemAnimator;

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
@property (nonatomic, readonly, nullable) SLKAnchorState *anchorState;

- (void)registerClass:(Class)cellClass forCellReuseIdentifier:(NSString *)identifier;
- (__kindof SLKListCell *)dequeueReusableCellWithIdentifier:(NSString *)identifier;

- (void)reloadData;
- (void)insertItemsAtIndices:(NSIndexSet *)indices;
- (void)deleteItemsAtIndices:(NSIndexSet *)indices;
- (void)reloadItemsAtIndices:(NSIndexSet *)indices;
- (void)reloadItemsAtIndices:(NSIndexSet *)indices payload:(nullable id)payload;
- (void)moveItemAtIndex:(NSInteger)index toIndex:(NSInteger)newIndex;
- (void)performBatchUpdates:(void (NS_NOESCAPE ^_Nullable)(void))updates completion:(void (^_Nullable)(BOOL finished))completion;
- (SLKListChanges *)applyChanges;

- (void)setEditing:(BOOL)editing animated:(BOOL)animated;
- (void)selectItemAtIndex:(NSInteger)index animated:(BOOL)animated;
- (void)deselectItemAtIndex:(NSInteger)index animated:(BOOL)animated;
- (void)closeSwipeActionsAnimated:(BOOL)animated;
- (void)restoreAnchorState:(SLKAnchorState *)state NS_SWIFT_NAME(restoreAnchorState(_:));

- (NSInteger)sectionForItemAtIndex:(NSInteger)index;
- (NSInteger)firstItemIndexInSection:(NSInteger)section NS_SWIFT_NAME(firstItemIndexInSection(_:));
- (CGRect)rectForHeaderInSection:(NSInteger)section;
- (void)scrollToSection:(NSInteger)section animated:(BOOL)animated NS_SWIFT_NAME(scrollToSection(_:animated:));

- (nullable __kindof SLKListCell *)cellForItemAtIndex:(NSInteger)index;
@property (nonatomic, readonly) NSArray<__kindof SLKListCell *> *visibleCells;
@property (nonatomic, readonly) NSRange visibleRange;
- (CGRect)rectForItemAtIndex:(NSInteger)index;

- (void)scrollToItemAtIndex:(NSInteger)index viewPosition:(CGFloat)viewPosition animated:(BOOL)animated;
- (void)scrollToStartAnimated:(BOOL)animated;
- (void)scrollToEndAnimated:(BOOL)animated;

@end

NS_ASSUME_NONNULL_END
