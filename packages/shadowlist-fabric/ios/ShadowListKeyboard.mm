#import "ShadowListKeyboard.h"

#import "ShadowListCompat.h"

/*
 * macOS has no software keyboard, so the observers below are iOS only.
 */
@implementation ShadowListKeyboard {
  /*
   * How many useKeyboardAnimation users are active. We attach on the first and detach
   * after the last, so one user unmounting never cuts off another.
   */
  NSInteger _enabledCount;
  CGFloat _current;       // Last height we sent, in dp.
  CGFloat _targetHeight;  // Full keyboard height for the running transition, in dp.
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
  // Observers and the display link are main thread only, but RN may call this from another thread.
  if ([NSThread isMainThread]) {
    [self applyEnabled:enabled];
  } else {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self applyEnabled:enabled];
    });
  }
}

/*
 * Counted, so calling it repeatedly is safe. Attach on the first enable and detach after
 * the last disable. Main thread only.
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
 * The display link retains us, so dealloc may run late on teardown. Clean up here instead.
 * Never use dispatch_sync: the main thread is waiting on module invalidation and it deadlocks.
 * The block keeps self alive until cleanup runs.
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
  // Nothing to observe on macOS, but keep the count in step.
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

  // How much of the screen the keyboard covers, in dp.
  CGFloat screenHeight = UIScreen.mainScreen.bounds.size.height;
  CGFloat height = hiding ? 0 : MAX(0, screenHeight - endFrame.origin.y);
  /*
   * Keep the full height during a hide so progress eases from 1 to 0 instead of reading 0
   * the whole time. It resets once the hide finishes or the next show replaces it.
   */
  if (!hiding && height > 0) {
    _targetHeight = height;
  }

  if (duration <= 0) {
    // No animation, so jump straight to the new height.
    [self stopDisplayLink];
    [self emitHeight:height];
    if (hiding) {
      _targetHeight = 0;
    }
    return;
  }

  // Animate from the current height to the new one over the reported duration.
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
  // Cubic ease out.
  CGFloat eased = 1 - pow(1 - time, 3);
  CGFloat value = _animationFrom + (_animationTo - _animationFrom) * eased;
  [self emitHeight:value];
  if (time >= 1.0) {
    [self stopDisplayLink];
    if (_animationTo <= 0) {
      // The hide is done. Reset so the next hide does not divide by a stale height.
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
