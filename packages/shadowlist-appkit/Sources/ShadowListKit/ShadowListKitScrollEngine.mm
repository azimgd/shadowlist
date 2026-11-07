#import <ShadowListKit/ShadowListKitScrollEngine.h>
#import <ShadowListKit/ShadowListKitListView+Testing.h>

#import <QuartzCore/QuartzCore.h>

#include <shadowlist-core/host/ScrollPhysics.hpp>

using namespace azimgd::shadowlist;

/*
 * The engine's display link target. It holds the engine weakly, which lets the engine go away
 * while the link exists.
 */
@interface ShadowListKitScrollEngineTarget : NSObject
@property (nonatomic, weak) ShadowListKitScrollEngine *engine;
@end

@interface ShadowListKitScrollEngine ()
- (void)frame;
@end

@implementation ShadowListKitScrollEngineTarget

- (void)tick:(CADisplayLink *)link
{
  ShadowListKitScrollEngine *engine = _engine;
  if (!engine) {
    [link invalidate];
    return;
  }
  [engine frame];
}

@end

static WheelPhase ShadowListKitWheelPhase(NSEventPhase phase)
{
  switch (phase) {
    case NSEventPhaseMayBegin:
      return WheelPhase::MayBegin;
    case NSEventPhaseBegan:
      return WheelPhase::Began;
    case NSEventPhaseChanged:
      return WheelPhase::Changed;
    case NSEventPhaseStationary:
      return WheelPhase::Stationary;
    case NSEventPhaseEnded:
      return WheelPhase::Ended;
    case NSEventPhaseCancelled:
      return WheelPhase::Cancelled;
    default:
      return WheelPhase::None;
  }
}

@implementation ShadowListKitScrollEngine {
  __weak NSView *_view;
  ScrollPhysics _physics;
  CADisplayLink *_link;

  /*
   * Added to the media clock, which lets scripted checks run frames without waiting.
   */
  CFTimeInterval _clockShift;
}

- (instancetype)initWithView:(NSView *)view
{
  if (self = [super init]) {
    _view = view;
    _axes = ShadowListKitScrollAxesVertical;
    _lineScroll = SCROLL_WHEEL_LINE;
    _physics.setAxes(false, true);
    __weak ShadowListKitScrollEngine *weakSelf = self;
    _physics.setSnapOffset([weakSelf](double& x, double& y) {
      ShadowListKitScrollEngine *engine = weakSelf;
      id<ShadowListKitScrollEngineDelegate> delegate = engine.delegate;
      if (![delegate respondsToSelector:@selector(scrollEngine:restingOffsetForOffset:)]) {
        return;
      }
      CGPoint resting = [delegate scrollEngine:engine restingOffsetForOffset:CGPointMake((CGFloat)x, (CGFloat)y)];
      x = resting.x;
      y = resting.y;
    });
  }
  return self;
}

- (void)dealloc
{
  [_link invalidate];
}

#pragma mark - Properties

- (void)setAxes:(ShadowListKitScrollAxes)axes
{
  _axes = axes;
  _physics.setAxes((axes & ShadowListKitScrollAxesHorizontal) != 0, (axes & ShadowListKitScrollAxesVertical) != 0);
}

- (CGPoint)offset
{
  return CGPointMake((CGFloat)_physics.getOffsetX(), (CGFloat)_physics.getOffsetY());
}

- (void)setOffset:(CGPoint)offset
{
  _physics.setOffset(offset.x, offset.y);
}

- (void)setContentSize:(CGSize)contentSize
{
  _contentSize = contentSize;
  [self updateLimits];
}

- (void)setViewportSize:(CGSize)viewportSize
{
  _viewportSize = viewportSize;
  [self updateLimits];
}

- (void)updateLimits
{
  _physics.setLimits(_contentSize.width - _viewportSize.width, _contentSize.height - _viewportSize.height,
    _viewportSize.width, _viewportSize.height);
}

- (CGPoint)maxOffset
{
  return CGPointMake((CGFloat)_physics.getMaxX(), (CGFloat)_physics.getMaxY());
}

- (ShadowListKitScrollPhase)phase
{
  return static_cast<ShadowListKitScrollPhase>(_physics.getPhase());
}

- (BOOL)isAnimating
{
  return _physics.isAnimating();
}

#pragma mark - Events

- (BOOL)handleScrollWheel:(NSEvent *)event
{
  WheelEvent wheel;
  wheel.phase = ShadowListKitWheelPhase(event.phase);
  wheel.momentum = ShadowListKitWheelPhase(event.momentumPhase);
  double scale = event.hasPreciseScrollingDeltas ? 1.0 : _lineScroll;
  wheel.deltaX = -event.scrollingDeltaX * scale;
  wheel.deltaY = -event.scrollingDeltaY * scale;
  ScrollStep step = _physics.wheel(wheel, [self now]);
  [self report:step];
  [self updateLink];
  return step.handled;
}

- (void)animateToOffset:(CGPoint)offset
{
  ScrollStep step = _physics.animateTo(offset.x, offset.y, [self now]);
  [self report:step];
  [self updateLink];
}

- (void)stopScrolling
{
  [self report:_physics.stop()];
  [self updateLink];
}

#pragma mark - Frames

- (double)now
{
  return (CACurrentMediaTime() + _clockShift) * 1000.0;
}

- (void)frame
{
  [self report:_physics.step([self now])];
  [self updateLink];
}

/*
 * Run frames only while something moves or waits.
 */
- (void)updateLink
{
  BOOL needed = _physics.needsFrames();
  if (!needed) {
    _link.paused = YES;
    return;
  }
  NSView *view = _view;
  if (!_link && view) {
    ShadowListKitScrollEngineTarget *target = [ShadowListKitScrollEngineTarget new];
    target.engine = self;
    _link = [view displayLinkWithTarget:target selector:@selector(tick:)];
    [_link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
  }
  _link.paused = NO;
}

/*
 * Tell the delegate what a step changed: a phase that starts before the move, one that ends
 * after it.
 */
- (void)report:(const ScrollStep&)step
{
  id<ShadowListKitScrollEngineDelegate> delegate = _delegate;
  BOOL starts = step.phaseChanged && _physics.getPhase() != ScrollPhase::Idle;
  BOOL reportsPhase = [delegate respondsToSelector:@selector(scrollEngine:didChangePhase:)];
  if (starts && reportsPhase) {
    [delegate scrollEngine:self didChangePhase:self.phase];
  }
  if (step.moved) {
    [delegate scrollEngine:self didScrollToOffset:self.offset];
  }
  if (step.phaseChanged && !starts && reportsPhase) {
    [delegate scrollEngine:self didChangePhase:self.phase];
  }
  if (step.animationEnded && [delegate respondsToSelector:@selector(scrollEngineDidEndAnimation:)]) {
    [delegate scrollEngineDidEndAnimation:self];
  }
}

@end

#pragma mark - Testing

NSEvent *ShadowListKitScrollWheelEvent(CGPoint delta, NSEventPhase phase, NSEventPhase momentumPhase)
{
  CGEventRef wheel = CGEventCreateScrollWheelEvent2(NULL, kCGScrollEventUnitPixel, 2, (int32_t)delta.y,
    (int32_t)delta.x, 0);
  int64_t scrollPhase = 0;
  switch (phase) {
    case NSEventPhaseBegan:
      scrollPhase = kCGScrollPhaseBegan;
      break;
    case NSEventPhaseChanged:
    case NSEventPhaseStationary:
      scrollPhase = kCGScrollPhaseChanged;
      break;
    case NSEventPhaseEnded:
      scrollPhase = kCGScrollPhaseEnded;
      break;
    case NSEventPhaseCancelled:
      scrollPhase = kCGScrollPhaseCancelled;
      break;
    case NSEventPhaseMayBegin:
      scrollPhase = kCGScrollPhaseMayBegin;
      break;
    default:
      break;
  }
  int64_t momentum = kCGMomentumScrollPhaseNone;
  switch (momentumPhase) {
    case NSEventPhaseBegan:
      momentum = kCGMomentumScrollPhaseBegin;
      break;
    case NSEventPhaseChanged:
    case NSEventPhaseStationary:
      momentum = kCGMomentumScrollPhaseContinue;
      break;
    case NSEventPhaseEnded:
    case NSEventPhaseCancelled:
      momentum = kCGMomentumScrollPhaseEnd;
      break;
    default:
      break;
  }
  CGEventSetIntegerValueField(wheel, kCGScrollWheelEventScrollPhase, scrollPhase);
  CGEventSetIntegerValueField(wheel, kCGScrollWheelEventMomentumPhase, momentum);
  NSEvent *event = [NSEvent eventWithCGEvent:wheel];
  CFRelease(wheel);
  return event;
}

@implementation ShadowListKitScrollEngine (Testing)

- (void)shadowListKit_runFramesFor:(NSTimeInterval)duration
{
  for (NSTimeInterval run = 0; run < duration; run += SCROLL_FRAME_MS / 1000.0) {
    _clockShift += SCROLL_FRAME_MS / 1000.0;
    [self frame];
  }
}

@end
