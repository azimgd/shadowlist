#pragma once

#import <ShadowListKit/ShadowListKitListView.h>
#import "ShadowListKitCoreBridge.h"
#import "ShadowListKitListCell+Private.h"

#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListSections.hpp>
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
extern const char ShadowListKitListViewCommandsLink;
extern const char ShadowListKitListViewDataLink;
extern const char ShadowListKitListViewSectionsLink;
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
 * applyChanges saw, by key. _ignoresMomentum drops the momentum of a wheel gesture a scroll command
 * stopped until it ends.
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
  BOOL _ignoresMomentum;
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
 * rests.
 */
@interface ShadowListKitListView (Commands)

- (void)scrollToRow:(std::size_t)row viewPosition:(CGFloat)viewPosition animated:(BOOL)animated;
- (void)stopScrolling;
- (void)restorePendingAnchor;
- (void)invalidateRestorableStateIfSettled;

@end

NS_ASSUME_NONNULL_END
