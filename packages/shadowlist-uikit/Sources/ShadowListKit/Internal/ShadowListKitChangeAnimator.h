#import <UIKit/UIKit.h>

#include <string>
#include <vector>

@class ShadowListKitListView;
@class ShadowListKitListCell;

NS_ASSUME_NONNULL_BEGIN

/*
 * The insert and delete animations of a list with animatesChanges. A data change records
 * where every mounted row is on screen. After the layout pass that applies it, rows that stay
 * slide from there to their new place, new rows fade in and removed rows fade out where they
 * were. The core anchors the content as usual: rows that hold still on screen do not move.
 *
 * captureRemoved:inserted: runs before a change reaches the core. Several changes before one
 * layout add up, and the first one records the screen. fadeOutKey:cell: keeps a removed row's
 * cell on screen where it was and fades it out, then recycles it. It returns NO when the cell
 * should go back to the pool right away. run slides the rows that stay and fades in the new
 * ones after the layout pass.
 */
@interface ShadowListKitChangeAnimator : NSObject

- (instancetype)initWithList:(ShadowListKitListView *)list;
- (void)captureRemoved:(const std::vector<std::string>&)removed inserted:(const std::vector<std::string>&)inserted;
- (BOOL)fadeOutKey:(const std::string&)key cell:(ShadowListKitListCell *)cell;
- (void)run;

@end

NS_ASSUME_NONNULL_END
