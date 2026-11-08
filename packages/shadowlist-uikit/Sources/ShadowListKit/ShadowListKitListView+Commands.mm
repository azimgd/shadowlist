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
 * animated command eases toward its estimated target and lands through the core when it arrives.
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
 * The window saves the position of a list with an identifier. On the Mac it asks again once the
 * list comes to rest somewhere else.
 */
- (void)encodeRestorableStateWithCoder:(NSCoder *)coder
{
  [super encodeRestorableStateWithCoder:coder];
  ShadowListKitAnchorState *state = self.anchorState;
  if (state) {
    [coder encodeObject:state forKey:@"ShadowListKitAnchorState"];
  }
}

- (void)restoreAnchorStateFromCoder:(NSCoder *)coder
{
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
  if (animated && [self animatesCommands] && row < _driver.getRowCount()) {
    // Animate to the estimate, then let the core land exactly when the animation ends.
    double target = _driver.animatedTargetOffset(row, viewPosition, _windowAlong, [self maxOffset]);
    [self animateToOffset:target landing:{ScrollLanding::Target::Row, row, viewPosition}];
    return;
  }
  [self stopScrolling];
  _driver.scrollToRow(row, viewPosition);
  [self runCommandNow];
  [self endCommandAnimated:animated];
}

- (void)scrollToStartAnimated:(BOOL)animated
{
  if (animated && [self animatesCommands]) {
    // The header shows at the very start, which landing on row 0 would scroll past.
    [self animateToOffset:0 landing:{ScrollLanding::Target::Start, 0, 0}];
    return;
  }
  [self stopScrolling];
  _driver.scrollToStart();
  [self runCommandNow];
  [self endCommandAnimated:animated];
}

- (void)scrollToEndAnimated:(BOOL)animated
{
  if (animated && [self animatesCommands]) {
    [self animateToOffset:[self maxOffset] landing:{ScrollLanding::Target::End, 0, 0}];
    return;
  }
  [self stopScrolling];
  _driver.scrollToEnd();
  [self runCommandNow];
  [self endCommandAnimated:animated];
}

/*
 * Stop momentum, a spring back and any animated command still on its way. Otherwise they keep
 * writing the offset over a command that runs now.
 */
- (void)stopScrolling
{
  [self stopScrollMotion];
  _driver.cancelLanding();
}

- (void)runCommandNow
{
  [self invalidateFrame];
  ShadowListKitLayoutNow(self);
}

#pragma mark - Platform

#if TARGET_OS_OSX

- (void)restoreStateWithCoder:(NSCoder *)coder
{
  [super restoreStateWithCoder:coder];
  [self restoreAnchorStateFromCoder:coder];
}

- (void)invalidateRestorableStateIfSettled
{
  if (self.identifier && self.scrollPhase == ShadowListKitScrollPhaseIdle) {
    [self invalidateRestorableState];
  }
}

/*
 * A driven list's commands move at once, and the host animates its own scrolling.
 */
- (BOOL)animatesCommands
{
  return !self.scrollDriven;
}

/*
 * A driven list's command moved at once. An animated one still reports its end.
 */
- (void)endCommandAnimated:(BOOL)animated
{
  if (animated && [_userDelegate respondsToSelector:@selector(listViewDidEndScrollingAnimation:)]) {
    [_userDelegate listViewDidEndScrollingAnimation:self];
  }
  [self invalidateRestorableStateIfSettled];
}

/*
 * A driven list's host stops its own scrolling.
 */
- (void)stopScrollMotion
{
  if (!self.scrollDriven) {
    [_engine stopScrolling];
  }
}

#else

- (void)decodeRestorableStateWithCoder:(NSCoder *)coder
{
  [super decodeRestorableStateWithCoder:coder];
  [self restoreAnchorStateFromCoder:coder];
}

/*
 * UIKit reads the saved position when it saves the scene. It needs no word of a change.
 */
- (void)invalidateRestorableStateIfSettled
{
}

- (BOOL)animatesCommands
{
  return YES;
}

/*
 * The scroll view tells the delegate when an animated command ends.
 */
- (void)endCommandAnimated:(BOOL)animated
{
}

- (void)animateToOffset:(double)offset landing:(ScrollLanding)landing
{
  _driver.setLanding(landing);
  [self setContentOffset:[self contentOffsetAt:offset] animated:YES];
}

- (void)stopScrollMotion
{
  [self setContentOffset:self.contentOffset animated:NO];
}

#endif

@end
#pragma clang diagnostic pop
