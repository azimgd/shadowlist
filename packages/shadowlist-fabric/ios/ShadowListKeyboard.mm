#import "ShadowListKeyboard.h"

#import "ShadowListCompat.h"

/*
 * The system keyboard is an iOS concept; there is no software keyboard on macOS, so the
 * keyboard observation below compiles only on iOS and setEnabled: is a no-op on macOS.
 */
@implementation ShadowListKeyboard {
  /*
   * Reference count of active useKeyboardAnimation() consumers. The observer/display link are
   * attached only on the 0->1 transition and detached only on the ->0 transition, so one
   * consumer's unmount never tears down another concurrent consumer's subscription.
   */
  NSInteger _enabledCount;
  CGFloat _current;       // last emitted height (dp)
  CGFloat _targetHeight;  // full keyboard height for the in-flight transition (dp)
#if !TARGET_OS_OSX
  CADisplayLink *_displayLink;
  CFTimeInterval _animationStart;
  CFTimeInterval _animationDuration;
  CGFloat _animationFrom;
  CGFloat _animationTo;
#endif
}

RCT_EXPORT_MODULE()

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
  return std::make_shared<facebook::react::NativeShadowListKeyboardSpecJSI>(params);
}

+ (BOOL)requiresMainQueueSetup
{
  return YES;
}

#pragma mark - Spec

#if !TARGET_OS_OSX
- (void)setEnabled:(BOOL)enabled
{
  /*
   * NSNotificationCenter add/removeObserver and CADisplayLink start/invalidate are only safe
   * on the main thread; RN may invoke this TurboModule method from a background thread.
   */
  if ([NSThread isMainThread]) {
    [self applyEnabled:enabled];
  } else {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self applyEnabled:enabled];
    });
  }
}

/*
 * Reference-counted per the TS spec ("safe to call repeatedly"): only actually attach the
 * observer/display link on the 0->1 transition, only detach on the ->0 transition. Must be
 * called on the main thread.
 */
- (void)applyEnabled:(BOOL)enabled
{
  if (enabled) {
    _enabledCount++;
    if (_enabledCount != 1) {
      return;
    }
  } else {
    if (_enabledCount == 0) {
      return;
    }
    if (--_enabledCount != 0) {
      return;
    }
  }

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

/*
 * RCTInvalidating: CADisplayLink retains its target, so -dealloc isn't guaranteed to run
 * promptly around bridge teardown. Clean up on the main thread, but never via dispatch_sync:
 * during teardown the main thread waits on module invalidation (RCTTurboModuleManager), so a
 * sync hop deadlocks. The block retains self, keeping the module alive until cleanup runs.
 */
- (void)invalidate
{
  void (^cleanup)(void) = ^{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [self stopDisplayLink];
    self->_enabledCount = 0;
  };
  if ([NSThread isMainThread]) {
    cleanup();
  } else {
    dispatch_async(dispatch_get_main_queue(), cleanup);
  }
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [_displayLink invalidate];
  _displayLink = nil;
}
#else
- (void)setEnabled:(BOOL)enabled
{
  // No software keyboard on macOS; nothing to observe, but keep the reference count consistent.
  if (enabled) {
    _enabledCount++;
  } else if (_enabledCount > 0) {
    _enabledCount--;
  }
}

- (void)invalidate
{
  _enabledCount = 0;
}
#endif // !TARGET_OS_OSX

#if !TARGET_OS_OSX
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

  // Height of the keyboard overlapping the screen, in dp.
  CGFloat screenHeight = UIScreen.mainScreen.bounds.size.height;
  CGFloat height = hiding ? 0 : MAX(0, screenHeight - endFrame.origin.y);
  /*
   * Keep _targetHeight at the last known full keyboard height throughout a hide transition so
   * that emitHeight's progress (height / _targetHeight) ramps 1.0 -> 0.0 instead of reading 0.0
   * for the whole animation. It's reset to 0 once the hide transition actually completes, or
   * overwritten by the next genuine show above.
   */
  if (!hiding && height > 0) {
    _targetHeight = height;
  }

  if (duration <= 0) {
    // No animation reported: jump straight to the value.
    [self stopDisplayLink];
    [self emitHeight:height];
    if (hiding) {
      _targetHeight = 0;
    }
    return;
  }

  // Interpolate from current to target over the reported duration.
  _animationFrom = _current;
  _animationTo = height;
  _animationDuration = duration;
  _animationStart = CACurrentMediaTime();
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
  CFTimeInterval elapsed = CACurrentMediaTime() - _animationStart;
  CGFloat time = _animationDuration > 0 ? MIN(1.0, elapsed / _animationDuration) : 1.0;
  // Ease-out cubic.
  CGFloat eased = 1 - pow(1 - time, 3);
  CGFloat value = _animationFrom + (_animationTo - _animationFrom) * eased;
  [self emitHeight:value];
  if (time >= 1.0) {
    [self stopDisplayLink];
    if (_animationTo <= 0) {
      /*
       * Hide transition finished: reset so the next hide notification's progress ramps
       * correctly instead of dividing against a stale target.
       */
      _targetHeight = 0;
    }
  }
}

#pragma mark - Emit

- (void)emitHeight:(CGFloat)height
{
  _current = height;
  CGFloat progress = _targetHeight > 0 ? MIN(1.0, MAX(0.0, height / _targetHeight)) : 0.0;
  [self emitOnKeyboardMove:@{ @"height" : @(height), @"progress" : @(progress) }];
}
#endif // !TARGET_OS_OSX

@end
