#pragma once

#import "ShadowListView.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * The lists that share one dragBoardId, and the row being dragged between them. Each list
 * holds its board strongly and the board holds its lists weakly. A board lives as long as one
 * of its lists does. The registry finds a board by id, made on first use, and forgets it when
 * its last list goes. Main thread only.
 *
 * A drag starts in its home list as usual. The core's DragBoard decides each frame whether
 * the row lifts out, comes back home or moves to another target, where its floating picture
 * goes, and how long a drop waits to land. Once the finger is over another list of the board,
 * the row's picture floats above the window as the overlay and the home list closes its gap.
 * The list the row hovers is the target and opens a gap for it. A drop there sends one
 * onDragEnd from the home list, and the overlay waits for the row to show up in the target's
 * data before it settles onto it.
 *
 * The properties below are the views of the current flight, which outlives the drag until
 * the row settles. lists holds every list on the board weakly. home is the list the row came
 * from and target the list it hovers or dropped in, both weak. overlay is the row's picture
 * in window coordinates. windowSize is the window's size at pickup: a rotation mid flight
 * cancels the drag, since the overlay sits in window coordinates. sidewaysScrollView is the
 * scroll view around the board the drag scrolled, which snaps to its page when the drag ends.
 * settling is set while the picture flies onto the landed row.
 */
@interface ShadowListDragBoard : NSObject

+ (ShadowListDragBoard *)boardWithId:(NSString *)boardId;

@property (nonatomic, readonly, copy) NSString *boardId;
@property (nonatomic, readonly) NSHashTable<ShadowListView *> *lists;
@property (nonatomic, weak, nullable) ShadowListView *home;
@property (nonatomic, weak, nullable) ShadowListView *target;
@property (nonatomic, strong, nullable) RCTUIView *overlay;
@property (nonatomic) CGSize windowSize;
@property (nonatomic, weak, nullable) RCTUIScrollView *sidewaysScrollView;
@property (nonatomic) BOOL settling;

- (void)endFlight;

@end

NS_ASSUME_NONNULL_END
