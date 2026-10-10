#import "Internal/ShadowListKitListView+Private.h"

#include <optional>

using namespace azimgd::shadowlist;

/*
 * Referenced from ShadowListKitListView.mm to keep this file linked without -ObjC.
 */
extern "C" const char ShadowListKitListViewCommandsLink = 0;

// The class interface in ShadowListKitListView.h declares the public members implemented here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"

/*
 * The saved position and the scroll commands. A command runs in the layout pass right away, or
 * animates to the estimate and lets the core land exactly when the animation ends.
 */
@implementation ShadowListKitListView (Commands)

#pragma mark - Saved position

- (ShadowListKitAnchorState *)anchorState
{
  std::optional<AnchorState> anchor = _driver.getAnchorState([self offset]);
  if (!anchor) {
    return _pendingAnchor;
  }
  return [[ShadowListKitAnchorState alloc] initWithKey:ShadowListKitString(anchor->key) offset:(CGFloat)anchor->offset];
}

- (void)restoreAnchorState:(ShadowListKitAnchorState *)state
{
  _pendingAnchor = state;
  [self restorePendingAnchor];
}

/*
 * Land the saved row once the data has it. Until then the position waits.
 */
- (void)restorePendingAnchor
{
  if (!_pendingAnchor) {
    return;
  }
  AnchorState anchor{ShadowListKitStdString(_pendingAnchor.key), (double)_pendingAnchor.offset};
  if (!_driver.restoreAnchorState(anchor)) {
    return;
  }
  _pendingAnchor = nil;
  [self stopScrolling];
  [self invalidateFrame];
}

- (void)encodeRestorableStateWithCoder:(NSCoder *)coder
{
  [super encodeRestorableStateWithCoder:coder];
  ShadowListKitAnchorState *state = self.anchorState;
  if (state) {
    [coder encodeObject:state forKey:@"ShadowListKitAnchorState"];
  }
}

- (void)decodeRestorableStateWithCoder:(NSCoder *)coder
{
  [super decodeRestorableStateWithCoder:coder];
  ShadowListKitAnchorState *state = [coder decodeObjectOfClass:[ShadowListKitAnchorState class] forKey:@"ShadowListKitAnchorState"];
  if (state) {
    [self restoreAnchorState:state];
  }
}

#pragma mark - Scroll commands

- (void)scrollToItemAtIndex:(NSInteger)index viewPosition:(CGFloat)viewPosition animated:(BOOL)animated
{
  NSInteger row = [self rowForItem:index];
  if (row == NSNotFound) {
    return;
  }
  [self scrollToRow:(std::size_t)row viewPosition:viewPosition animated:animated];
}

- (void)scrollToRow:(std::size_t)row viewPosition:(CGFloat)viewPosition animated:(BOOL)animated
{
  if (row >= _driver.getKeyCount()) {
    return;
  }
  if (animated && row < _driver.getRowCount()) {
    // Animate to the estimate, then let the core land exactly when the animation ends.
    double target = _driver.animatedTargetOffset(row, viewPosition, _windowAlong, [self maxOffset]);
    [self animateCommandTo:target landing:{ScrollLanding::Target::Row, row, viewPosition}];
    return;
  }
  [self stopScrolling];
  _driver.scrollToRow(row, viewPosition);
  [self runCommandNow];
}

- (void)scrollToStartAnimated:(BOOL)animated
{
  if (animated) {
    // The header shows at the very start, which landing on row 0 would scroll past.
    [self animateCommandTo:0 landing:{ScrollLanding::Target::Start, 0, 0}];
    return;
  }
  [self stopScrolling];
  _driver.scrollToStart();
  [self runCommandNow];
}

- (void)scrollToEndAnimated:(BOOL)animated
{
  if (animated) {
    [self animateCommandTo:[self maxOffset] landing:{ScrollLanding::Target::End, 0, 0}];
    return;
  }
  [self stopScrolling];
  _driver.scrollToEnd();
  [self runCommandNow];
}

- (void)animateCommandTo:(double)target landing:(ScrollLanding)landing
{
  _driver.setLanding(landing);
  [self setContentOffset:[self contentOffsetAt:target] animated:YES];
}

/*
 * Stop momentum and any animated command still on its way. Otherwise they keep writing
 * the offset over a command that runs now.
 */
- (void)stopScrolling
{
  [self setContentOffset:self.contentOffset animated:NO];
  _driver.cancelLanding();
}

- (void)runCommandNow
{
  [self invalidateFrame];
  [self layoutIfNeeded];
}

@end
#pragma clang diagnostic pop
