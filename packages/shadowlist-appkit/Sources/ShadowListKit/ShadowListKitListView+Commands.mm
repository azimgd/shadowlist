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
 * The saved position and the scroll commands. A command runs in the layout pass right away. An
 * animated command does the same.
 */
@implementation ShadowListKitListView (Commands)

#pragma mark - Saved position

/*
 * The item at the viewport start by key and how far the viewport is into it, or nil before
 * the first layout.
 */
- (ShadowListKitAnchorState *)anchorState
{
  std::optional<AnchorState> anchor = _driver.getAnchorState([self offset]);
  if (!anchor) {
    return _pendingAnchor;
  }
  return [[ShadowListKitAnchorState alloc] initWithKey:ShadowListKitString(anchor->key) offset:(CGFloat)anchor->offset];
}

/*
 * Land the saved item the same distance in again, now or once a reload brings its key.
 */
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
  [self invalidateRestorableStateIfSettled];
}

/*
 * The window saves the position of a list with an identifier. It asks again once the list
 * comes to rest somewhere else.
 */
- (void)encodeRestorableStateWithCoder:(NSCoder *)coder
{
  [super encodeRestorableStateWithCoder:coder];
  ShadowListKitAnchorState *state = self.anchorState;
  if (state) {
    [coder encodeObject:state forKey:@"ShadowListKitAnchorState"];
  }
}

- (void)restoreStateWithCoder:(NSCoder *)coder
{
  [super restoreStateWithCoder:coder];
  ShadowListKitAnchorState *state = [coder decodeObjectOfClass:[ShadowListKitAnchorState class] forKey:@"ShadowListKitAnchorState"];
  if (state) {
    [self restoreAnchorState:state];
  }
}

- (void)invalidateRestorableStateIfSettled
{
  if (self.identifier && self.scrollPhase == ShadowListKitScrollPhaseIdle) {
    [self invalidateRestorableState];
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
  [self stopScrolling];
  _driver.scrollToRow(row, viewPosition);
  [self runCommandNow];
  [self endCommandAnimated:animated];
}

- (void)scrollToStartAnimated:(BOOL)animated
{
  [self stopScrolling];
  _driver.scrollToStart();
  [self runCommandNow];
  [self endCommandAnimated:animated];
}

- (void)scrollToEndAnimated:(BOOL)animated
{
  [self stopScrolling];
  _driver.scrollToEnd();
  [self runCommandNow];
  [self endCommandAnimated:animated];
}

- (void)endCommandAnimated:(BOOL)animated
{
  if (animated && [_delegate respondsToSelector:@selector(listViewDidEndScrollingAnimation:)]) {
    [_delegate listViewDidEndScrollingAnimation:self];
  }
  [self invalidateRestorableStateIfSettled];
}

/*
 * Stop momentum and cancel a landing. Otherwise the wheel's momentum keeps writing the offset
 * over a command that runs now. A driven list's host stops its own.
 */
- (void)stopScrolling
{
  if (!self.scrollDriven && self.scrollPhase == ShadowListKitScrollPhaseSettling) {
    _ignoresMomentum = YES;
    self.scrollPhase = ShadowListKitScrollPhaseIdle;
  }
  _driver.cancelLanding();
}

- (void)runCommandNow
{
  [self invalidateFrame];
  [self layoutSubtreeIfNeeded];
}

@end
#pragma clang diagnostic pop
