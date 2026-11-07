#pragma once

#import <AppKit/AppKit.h>

#import <ShadowListKit/ShadowListKitListModels.h>

NS_ASSUME_NONNULL_BEGIN

@class ShadowListKitListView;

/*
 * The view that shows an item. It is flipped and layer backed, and its layer never animates a
 * change by itself. Subclasses lay out their subviews by hand in layout and report their height
 * in sizeThatFits:. Without an override, sizeThatFits: fits the cell's Auto Layout constraints to
 * the given width, which makes a cell built with constraints self-sizing. index is the item the
 * cell shows, or NSNotFound while it waits in the reuse pool and for a section header or footer.
 * prepareForReuse is called before the cell is handed out again by
 * dequeueReusableCellWithIdentifier:.
 *
 * The list sets highlighted while the pointer presses a selectable item, selected for selected
 * items, editing while the list is editing, hovered while the pointer is over the item and
 * emphasized while the list is the key window's first responder. Override the animated setters
 * to show them. They are called only when the value changes.
 */
@interface ShadowListKitListCell : NSView

@property (nonatomic, copy, readonly, nullable) NSString *reuseIdentifier;
@property (nonatomic, readonly) NSInteger index;
@property (nonatomic, getter=isHighlighted) BOOL highlighted;
@property (nonatomic, getter=isSelected) BOOL selected;
@property (nonatomic, getter=isEditing) BOOL editing;
@property (nonatomic, getter=isHovered) BOOL hovered;
@property (nonatomic, getter=isEmphasized) BOOL emphasized;

- (instancetype)initWithReuseIdentifier:(nullable NSString *)reuseIdentifier NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(NSRect)frame NS_UNAVAILABLE;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;
- (NSSize)sizeThatFits:(NSSize)size;
- (void)prepareForReuse NS_REQUIRES_SUPER;
- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated NS_REQUIRES_SUPER;
- (void)setSelected:(BOOL)selected animated:(BOOL)animated NS_REQUIRES_SUPER;
- (void)setEditing:(BOOL)editing animated:(BOOL)animated NS_REQUIRES_SUPER;
- (void)setHovered:(BOOL)hovered animated:(BOOL)animated NS_REQUIRES_SUPER;
- (void)setEmphasized:(BOOL)emphasized animated:(BOOL)animated NS_REQUIRES_SUPER;

@end

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
 * What moves the scroll position. Dragging is the reader's gesture or a host that drives the
 * offset by hand, Settling is momentum or an animation after it. The order matches the core's
 * ScrollPhase.
 */
typedef NS_ENUM(NSInteger, ShadowListKitScrollPhase) {
  ShadowListKitScrollPhaseIdle,
  ShadowListKitScrollPhaseDragging,
  ShadowListKitScrollPhaseSettling,
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

NS_SWIFT_UI_ACTOR
@protocol ShadowListKitListViewDelegate <NSObject>

@optional

- (void)listView:(ShadowListKitListView *)listView willDisplayCell:(ShadowListKitListCell *)cell atIndex:(NSInteger)index;
- (void)listView:(ShadowListKitListView *)listView didEndDisplayingCell:(ShadowListKitListCell *)cell atIndex:(NSInteger)index;

/*
 * Whether the separator below an item shows when showsSeparators is set. Separators only go
 * between items of one section.
 */
- (BOOL)listView:(ShadowListKitListView *)listView showsSeparatorAfterItemAtIndex:(NSInteger)index NS_SWIFT_NAME(listView(_:showsSeparatorAfterItemAt:));

/*
 * The scroll position came within startReachedThreshold or endReachedThreshold of an edge.
 */
- (void)listViewDidReachStart:(ShadowListKitListView *)listView;
- (void)listViewDidReachEnd:(ShadowListKitListView *)listView;

/*
 * The scroll events. listViewDidScroll: follows every change of contentOffset. The others follow
 * scrollPhase: dragging begins and ends, and the settling after it ends.
 * listViewDidEndScrollingAnimation: follows an animated scroll command.
 */
- (void)listViewDidScroll:(ShadowListKitListView *)listView;
- (void)listViewWillBeginDragging:(ShadowListKitListView *)listView;
- (void)listViewDidEndDragging:(ShadowListKitListView *)listView willDecelerate:(BOOL)decelerate;
- (void)listViewDidEndDecelerating:(ShadowListKitListView *)listView;
- (void)listViewDidEndScrollingAnimation:(ShadowListKitListView *)listView;

/*
 * A layout pass ended. The cells of the rows near the viewport are mounted and placed.
 */
- (void)listViewDidLayOut:(ShadowListKitListView *)listView;

/*
 * With scrollDriven, the content offset the list wants: a correction that keeps the visible
 * items still or the target of a scroll command. Set contentOffset to it inside this call. The
 * list lays out at the offset it has when the call returns.
 */
- (void)listView:(ShadowListKitListView *)listView didCorrectOffset:(CGPoint)contentOffset;

@end

/*
 * A virtualized list on the shadowlist core for AppKit. Items are placed by the core, which keeps
 * the visible content still while items are measured, inserted or deleted. Measurement, offset
 * corrections and mounting all happen in one layout pass on the main thread. A scroll frame
 * that stays inside the core's offset band does no core work at all.
 *
 * The list is a flipped, layer backed view that owns its scroll position: no NSScrollView. Its
 * cells sit in a content view moved by contentOffset. A ShadowListKitScrollEngine scrolls it with
 * the scroll wheel the way a scroll view does: trackpad gestures rubber band past the edges and
 * their momentum settles, a mouse wheel scrolls by lines. A gesture across the list's axis goes
 * to the next responder whole, momentum included, and so does a mouse wheel at an edge.
 * ShadowListKitScrollerView draws overlay scrollers over it.
 *
 * Layout:
 * - inverted: a chat style list that opens at its end and keeps the end in view while the
 *   reader is there. Items stay in data order, oldest first.
 * - followAppends: with inverted, items appended or growing while the reader rests at the end
 *   scroll into view, like an assistant reply. Off by default, which keeps the visible items in
 *   place.
 * - numberOfColumns: columns of a grid. Row r, counting section header and footer rows, goes
 *   into column r % numberOfColumns and each column stacks its own rows.
 * - estimatedItemSize: size along the scroll axis assumed for items not measured yet.
 * - overscan: how far past the viewport items are measured, in viewport sizes. Default 1.
 * - mountOverscan: how far past the viewport cells are mounted, in viewport sizes. Default 0.5.
 * - startReachedThreshold, endReachedThreshold: distances to an edge, in viewport sizes, that
 *   fire the reached callbacks. Default 1.
 * - snapToItem, snapAlignment: rest the scroll position on the item edge snapAlignment names
 *   once a gesture's momentum or a mouse wheel ends.
 *
 * Scrolling:
 * - contentOffset: the content point at the top left of the viewport. contentInset pads the
 *   content inside the list, and adjustedContentInset adds the safe area to it. At the start the
 *   offset along the axis is minus the leading inset.
 * - contentSize: the size of the content along the axis and the viewport across it.
 * - scrollEnabled: whether the scroll wheel scrolls the list. Without it, wheel events go to
 *   the next responder.
 * - scrollPhase, isTracking, isDragging, isDecelerating: what moves the offset now.
 * - scrollDriven: a host moves the list. The host sets contentOffset and scrollPhase, the list
 *   never moves itself, shows no scrollers and wheel events go to the next responder. An offset
 *   the list wants is reported to the delegate's listView:didCorrectOffset:, which sets it.
 * - setContentOffset:animated: eases to the offset and listViewDidEndScrollingAnimation: follows
 *   when it arrives. A gesture or another command cuts it short without the call.
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
 *   together in one layout, the way NSTableView takes them between beginUpdates and endUpdates:
 *   deletes, reloads and move sources are indices in the data before, inserts and move
 *   destinations in the data after. A batch that does not add up reloads everything. completion
 *   gets finished once the change is laid out.
 * - applyChanges reads every key, works out the inserts, deletes and moves against the keys the
 *   list holds, reloads items whose content version changed, and returns what it found.
 * - In a list with sections every change reads the sections and keys again, like reloadData.
 *
 * Separators: showsSeparators draws lines between items over the trailing edge of each item's
 * cell. The insets are along the cross axis from its start and end, and the thickness defaults
 * to one device pixel. The color follows the list's appearance. Prefetching:
 * prefetchDataSource hears which items come next.
 *
 * Sections: see the data source. sectionForItemAtIndex:, firstItemIndexInSection:,
 * rectForHeaderInSection:, rectForFooterInSection: and scrollToSection:animated: work in
 * sections. A grid places section header rows in a column slot like any item.
 *
 * Saving the position: anchorState is the item at the viewport start by key, and
 * restoreAnchorState: lands that item the same distance in again, now or once a reload brings
 * its key. The list saves it with the window's state restoration when it has an identifier.
 *
 * Geometry:
 * - visibleRange: the items overlapping the viewport, low to high.
 * - rectForItemAtIndex: the item's frame in content coordinates: from the start of the content,
 *   without the content insets or the scroll offset. Estimated when the item was not measured
 *   yet. The section rects use the same space.
 * - scrollToItemAtIndex:viewPosition:animated: brings an item into view. viewPosition is where
 *   it rests in the viewport, from 0 at the start to 1 at the end. The core keeps correcting
 *   until the item lands, even across estimates. An animated scroll command eases toward the
 *   item's estimated place, then the core lands it exactly and
 *   listViewDidEndScrollingAnimation: follows. A driven list's command moves at once.
 */
@interface ShadowListKitListView : NSView

@property (nonatomic, weak, nullable) id<ShadowListKitListViewDataSource> dataSource;
@property (nonatomic, weak, nullable) id<ShadowListKitListViewDelegate> delegate;

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
@property (nonatomic) ShadowListKitSnapAlignment snapAlignment;

@property (nonatomic, weak, nullable) id<ShadowListKitListViewPrefetchDataSource> prefetchDataSource;

@property (nonatomic) BOOL allowsSelection;

@property (nonatomic) BOOL showsSeparators;
@property (nonatomic, strong, null_resettable) NSColor *separatorColor;
@property (nonatomic) CGFloat separatorInsetStart;
@property (nonatomic) CGFloat separatorInsetEnd;
@property (nonatomic) CGFloat separatorThickness;

@property (nonatomic) CGPoint contentOffset;
@property (nonatomic, readonly) CGSize contentSize;
@property (nonatomic) NSEdgeInsets contentInset;
@property (nonatomic, readonly) NSEdgeInsets adjustedContentInset;
@property (nonatomic, getter=isScrollEnabled) BOOL scrollEnabled;
@property (nonatomic) BOOL scrollDriven;
@property (nonatomic) ShadowListKitScrollPhase scrollPhase;
@property (nonatomic, readonly, getter=isTracking) BOOL tracking;
@property (nonatomic, readonly, getter=isDragging) BOOL dragging;
@property (nonatomic, readonly, getter=isDecelerating) BOOL decelerating;

@property (nonatomic, readonly) NSInteger numberOfSections;
@property (nonatomic, readonly, nullable) ShadowListKitAnchorState *anchorState;

- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated;

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
