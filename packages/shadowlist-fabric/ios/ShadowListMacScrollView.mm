#import "ShadowListMacScrollView.h"

#if TARGET_OS_OSX
static const NSTimeInterval SLWheelIdleDelay = 0.15;

@implementation ShadowListMacScrollView {
  ShadowListMacScrollPhase _phase;
  BOOL _liveScroll;
  BOOL _ignoreMomentum;
  BOOL _forwardingGesture;
  BOOL _axisDecided;
}

@dynamic delegate;

- (instancetype)initWithFrame:(NSRect)frame
{
  if ((self = [super initWithFrame:frame])) {
    self.automaticallyAdjustsContentInsets = NO;
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    [center addObserver:self selector:@selector(beginLiveScroll:)
                   name:NSScrollViewWillStartLiveScrollNotification object:self];
    [center addObserver:self selector:@selector(endLiveScroll:)
                   name:NSScrollViewDidEndLiveScrollNotification object:self];
  }
  return self;
}

- (void)dealloc
{
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)beginLiveScroll:(NSNotification *)notification
{
  _ignoreMomentum = NO;
  if (_phase != ShadowListMacScrollPhaseMomentum) {
    _phase = ShadowListMacScrollPhaseTracking;
  }
  [self notifyLiveScrollBegan];
}

- (void)notifyLiveScrollBegan
{
  _liveScroll = YES;
  [self.delegate shadowListScrollWillBegin];
}

- (void)endLiveScroll:(NSNotification *)notification
{
  [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(endLiveScroll:) object:nil];
  _phase = ShadowListMacScrollPhaseIdle;
  if (!_liveScroll) {
    return;
  }
  _liveScroll = NO;
  [self.delegate shadowListScrollDidEnd];
}

- (void)setHorizontal:(BOOL)horizontal
{
  _horizontal = horizontal;
  // No rubber band across the axis, so a stray cross-axis delta never wobbles the list.
  self.horizontalScrollElasticity = horizontal ? NSScrollElasticityAutomatic : NSScrollElasticityNone;
  self.verticalScrollElasticity = horizontal ? NSScrollElasticityNone : NSScrollElasticityAutomatic;
}

/*
 * Whether a wheel event belongs to the enclosing scroll view. A trackpad gesture is judged by
 * its first event and keeps that answer through its momentum, as AppKit's own nested scroll
 * views do. A mouse wheel has no phases and is judged per event.
 */
- (BOOL)shouldForwardScrollWheel:(NSEvent *)event
{
  BOOL unphased = event.phase == NSEventPhaseNone && event.momentumPhase == NSEventPhaseNone;
  if (event.phase == NSEventPhaseMayBegin || event.phase == NSEventPhaseBegan || unphased) {
    _axisDecided = NO;
    _forwardingGesture = NO;
  }
  CGFloat along = fabs(_horizontal ? event.scrollingDeltaX : event.scrollingDeltaY);
  CGFloat across = fabs(_horizontal ? event.scrollingDeltaY : event.scrollingDeltaX);
  // A gesture can open with an empty event. Decide on the first one that moves.
  if (!_axisDecided && along + across > 0) {
    _axisDecided = YES;
    _forwardingGesture = across > along;
  }
  return _forwardingGesture;
}

- (void)scrollWheel:(NSEvent *)event
{
  if ([self shouldForwardScrollWheel:event]) {
    [self.nextResponder scrollWheel:event];
    return;
  }
  if (!self.scrollEnabled) {
    return;
  }
  BOOL momentum = event.momentumPhase != NSEventPhaseNone;
  BOOL momentumEnds = event.momentumPhase == NSEventPhaseEnded || event.momentumPhase == NSEventPhaseCancelled;
  if (!momentum) {
    _ignoreMomentum = NO;
  }
  if (_ignoreMomentum && momentum) {
    if (momentumEnds) {
      [self endLiveScroll:nil];
    }
    return;
  }
  BOOL unphased = !momentum && event.phase == NSEventPhaseNone;
  if (event.phase == NSEventPhaseBegan || (unphased && !_liveScroll)) {
    [self notifyLiveScrollBegan];
  }
  NSEventPhase touching = NSEventPhaseMayBegin | NSEventPhaseBegan | NSEventPhaseChanged | NSEventPhaseStationary;
  if (event.momentumPhase == NSEventPhaseBegan || event.momentumPhase == NSEventPhaseChanged) {
    _phase = ShadowListMacScrollPhaseMomentum;
  } else if (unphased || (event.phase & touching) != 0) {
    _phase = ShadowListMacScrollPhaseTracking;
  } else {
    _phase = ShadowListMacScrollPhaseIdle;
  }
  [super scrollWheel:event];
  if (unphased) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(endLiveScroll:) object:nil];
    [self performSelector:@selector(endLiveScroll:) withObject:nil afterDelay:SLWheelIdleDelay];
  } else if (momentumEnds || event.phase == NSEventPhaseCancelled) {
    [self endLiveScroll:nil];
  }
}

- (BOOL)stopMomentum
{
  BOOL moving = _phase == ShadowListMacScrollPhaseMomentum;
  _ignoreMomentum = YES;
  if (moving) {
    _phase = ShadowListMacScrollPhaseIdle;
  }
  [self.contentView.layer removeAllAnimations];
  return moving;
}

- (void)resetScroll
{
  [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(endLiveScroll:) object:nil];
  _liveScroll = NO;
  _phase = ShadowListMacScrollPhaseIdle;
  _ignoreMomentum = YES;
}

- (BOOL)acceptsFirstResponder
{
  return YES;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
  NSResponder *responder = self.window.firstResponder;
  BOOL focused = responder == self ||
    ([responder isKindOfClass:NSView.class] && [(NSView *)responder isDescendantOf:self]);
  if (focused && (event.modifierFlags & NSEventModifierFlagCommand) &&
      [event.charactersIgnoringModifiers.lowercaseString isEqualToString:@"r"]) {
    [self.delegate shadowListRefresh];
    return YES;
  }
  return [super performKeyEquivalent:event];
}
@end
#endif
