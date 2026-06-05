#import "ShadowlistKeyboard.h"

#import <UIKit/UIKit.h>

// ShadowlistKeyboard subclasses the generated NativeShadowlistKeyboardSpecBase (see
// the header), which provides emitOnKeyboardMove: and the _eventEmitterCallback the
// TurboModule infrastructure wires up.
@implementation ShadowlistKeyboard {
  BOOL _enabled;
  CGFloat _current;       // last emitted height (dp)
  CGFloat _targetHeight;  // full keyboard height for the in-flight transition (dp)
  CADisplayLink *_displayLink;
  CFTimeInterval _animStart;
  CFTimeInterval _animDuration;
  CGFloat _animFrom;
  CGFloat _animTo;
}

RCT_EXPORT_MODULE()

/*
 * Required for a New Architecture TurboModule: hand back the generated JSI binding so
 * the module is reachable as TurboModuleRegistry.getEnforcing('ShadowlistKeyboard').
 */
- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
  return std::make_shared<facebook::react::NativeShadowlistKeyboardSpecJSI>(params);
}

+ (BOOL)requiresMainQueueSetup
{
  return YES;
}

#pragma mark - Spec

- (void)setEnabled:(BOOL)enabled
{
  if (enabled == _enabled) {
    return;
  }
  _enabled = enabled;

  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  if (enabled) {
    [center addObserver:self
               selector:@selector(keyboardWillChangeFrame:)
                   name:UIKeyboardWillChangeFrameNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(keyboardWillHide:)
                   name:UIKeyboardWillHideNotification
                 object:nil];
  } else {
    [center removeObserver:self];
    [self stopDisplayLink];
  }
}

#pragma mark - Keyboard notifications

- (void)keyboardWillChangeFrame:(NSNotification *)notification
{
  [self handleKeyboardNotification:notification hiding:NO];
}

- (void)keyboardWillHide:(NSNotification *)notification
{
  [self handleKeyboardNotification:notification hiding:YES];
}

- (void)handleKeyboardNotification:(NSNotification *)notification hiding:(BOOL)hiding
{
  NSDictionary *info = notification.userInfo;
  CGRect endFrame = [info[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  NSTimeInterval duration = [info[UIKeyboardAnimationDurationUserInfoKey] doubleValue];

  // Height of the keyboard overlapping the screen, in points (== dp on iOS).
  CGFloat screenHeight = UIScreen.mainScreen.bounds.size.height;
  CGFloat height = hiding ? 0 : MAX(0, screenHeight - endFrame.origin.y);
  if (hiding) {
    _targetHeight = 0;
  } else if (height > 0) {
    _targetHeight = height;
  }

  if (duration <= 0) {
    // No animation reported: jump straight to the value.
    [self stopDisplayLink];
    [self emitHeight:height];
    return;
  }

  // Drive a CADisplayLink to interpolate from the current value to the target over
  // the system's reported duration, so our stream matches the keyboard's animation.
  _animFrom = _current;
  _animTo = height;
  _animDuration = duration;
  _animStart = CACurrentMediaTime();
  [self startDisplayLink];
}

#pragma mark - Display link

- (void)startDisplayLink
{
  if (_displayLink) {
    return;
  }
  _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(onFrame:)];
  [_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

- (void)stopDisplayLink
{
  [_displayLink invalidate];
  _displayLink = nil;
}

- (void)onFrame:(CADisplayLink *)link
{
  CFTimeInterval elapsed = CACurrentMediaTime() - _animStart;
  CGFloat t = _animDuration > 0 ? MIN(1.0, elapsed / _animDuration) : 1.0;
  // Ease-out, close to the keyboard curve. (The exact system curve is private.)
  CGFloat eased = 1 - pow(1 - t, 3);
  CGFloat value = _animFrom + (_animTo - _animFrom) * eased;
  [self emitHeight:value];
  if (t >= 1.0) {
    [self stopDisplayLink];
  }
}

#pragma mark - Emit

- (void)emitHeight:(CGFloat)height
{
  _current = height;
  CGFloat progress = _targetHeight > 0 ? MIN(1.0, MAX(0.0, height / _targetHeight)) : 0.0;
  // emitOnKeyboardMove is generated from the onKeyboardMove EventEmitter in the spec.
  [self emitOnKeyboardMove:@{ @"height" : @(height), @"progress" : @(progress) }];
}

@end
