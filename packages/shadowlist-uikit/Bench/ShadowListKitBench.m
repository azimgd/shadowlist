#import "ShadowListKitBench.h"

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <mach/mach.h>
#import <sys/resource.h>

static thread_act_t ShadowListKitBenchMainThread;

/*
 * Pause between two axis runs, which lets the last run's work settle.
 */
static const NSTimeInterval SHADOWLIST_KIT_BENCH_RUN_GAP = 0.5;

/*
 * A frame counts as blank past this share of the viewport.
 */
static const double SHADOWLIST_KIT_BENCH_BLANK_FRAME = 0.02;

static double ShadowListKitBenchThreadSeconds(thread_act_t thread)
{
  thread_basic_info_data_t info;
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  if (thread_info(thread, THREAD_BASIC_INFO, (thread_info_t)&info, &count) != KERN_SUCCESS) {
    return 0;
  }
  return info.user_time.seconds + info.user_time.microseconds / 1e6 + info.system_time.seconds + info.system_time.microseconds / 1e6;
}

static double ShadowListKitBenchProcessSeconds(void)
{
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
}

static double ShadowListKitBenchFootprintMB(void)
{
  task_vm_info_data_t info;
  mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) {
    return 0;
  }
  return info.phys_footprint / 1048576.0;
}

@implementation ShadowListKitBench {
  NSArray<NSString *> *_axes;
  NSUInteger _run;
  NSString *_axis;
  UIScrollView *_vertical;
  UIScrollView *_horizontal;
  UIScrollView *_coverage;
  id<ShadowListKitBenchProbe> _probe;
  BOOL _drivesX;
  BOOL _drivesY;
  CADisplayLink *_link;
  CFTimeInterval _last;
  CFTimeInterval _legStart;
  double _speed;
  double _seconds;
  int _directionX;
  int _directionY;
  int _legs;
  double _positionX;
  double _positionY;
  NSMutableArray<NSNumber *> *_intervals;
  double _nominal;
  double _mainStart;
  double _processStart;
  double _peakMB;
  NSUInteger _frame;
  double _blankSum;
  double _blankMax;
  NSUInteger _blankSamples;
  NSUInteger _blankFrames;
  double _contentBlankSum;
  double _contentBlankMax;
  NSUInteger _contentBlankFrames;
  double _travel;
}

+ (void)load
{
  ShadowListKitBenchMainThread = mach_thread_self();
  if (![[NSUserDefaults standardUserDefaults] stringForKey:@"SLBench"]) {
    return;
  }
  static ShadowListKitBench *bench;
  [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification
                                                    object:nil
                                                     queue:NSOperationQueue.mainQueue
                                                usingBlock:^(NSNotification *note) {
                                                  if (bench) {
                                                    return;
                                                  }
                                                  bench = [ShadowListKitBench new];
                                                  double delay = [self setting:@"SLBenchDelay" fallback:3];
                                                  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                                                    dispatch_get_main_queue(), ^{
                                                      [bench start];
                                                    });
                                                }];
}

+ (double)setting:(NSString *)name fallback:(double)fallback
{
  NSString *value = [[NSUserDefaults standardUserDefaults] stringForKey:name];
  return value.length > 0 ? value.doubleValue : fallback;
}

+ (void)collectScrollViews:(UIView *)view into:(NSMutableArray<UIScrollView *> *)found
{
  if ([view isKindOfClass:[UIScrollView class]] && !view.hidden && view.window) {
    [found addObject:(UIScrollView *)view];
  }
  for (UIView *subview in view.subviews) {
    [self collectScrollViews:subview into:found];
  }
}

/*
 * The scroll view with the most content along an axis, wider than half the screen.
 */
+ (UIScrollView *)findScrollViewAlongX:(BOOL)alongX
{
  NSMutableArray<UIScrollView *> *found = [NSMutableArray new];
  for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
    if (![scene isKindOfClass:[UIWindowScene class]]) {
      continue;
    }
    for (UIWindow *window in ((UIWindowScene *)scene).windows) {
      [self collectScrollViews:window into:found];
    }
  }
  UIScrollView *best = nil;
  CGFloat bestRange = 0;
  for (UIScrollView *scrollView in found) {
    CGFloat range = alongX ? scrollView.contentSize.width - scrollView.bounds.size.width
                           : scrollView.contentSize.height - scrollView.bounds.size.height;
    if (scrollView.bounds.size.width > scrollView.window.bounds.size.width * 0.5 && range > bestRange) {
      best = scrollView;
      bestRange = range;
    }
  }
  return best;
}

/*
 * The first of the views and their superviews that answers the probe.
 */
+ (id<ShadowListKitBenchProbe>)findProbeFrom:(NSArray<UIView *> *)views
{
  for (UIView *start in views) {
    for (UIView *view = start; view; view = view.superview) {
      if ([view respondsToSelector:@selector(shadowListKit_benchBlankFraction)]) {
        return (id<ShadowListKitBenchProbe>)view;
      }
    }
  }
  return nil;
}

- (CGFloat)minOffsetOf:(UIScrollView *)scrollView alongX:(BOOL)alongX
{
  UIEdgeInsets inset = scrollView.adjustedContentInset;
  return alongX ? -inset.left : -inset.top;
}

- (CGFloat)maxOffsetOf:(UIScrollView *)scrollView alongX:(BOOL)alongX
{
  UIEdgeInsets inset = scrollView.adjustedContentInset;
  if (alongX) {
    return MAX(-inset.left, scrollView.contentSize.width + inset.right - scrollView.bounds.size.width);
  }
  return MAX(-inset.top, scrollView.contentSize.height + inset.bottom - scrollView.bounds.size.height);
}

/*
 * Go away from the edge the view opened at: forward from the start, back from the end.
 */
- (int)directionOf:(UIScrollView *)scrollView alongX:(BOOL)alongX
{
  CGFloat offset = alongX ? scrollView.contentOffset.x : scrollView.contentOffset.y;
  CGFloat middle = ([self minOffsetOf:scrollView alongX:alongX] + [self maxOffsetOf:scrollView alongX:alongX]) / 2;
  return offset > middle ? -1 : 1;
}

- (void)start
{
  NSString *axes = [[NSUserDefaults standardUserDefaults] stringForKey:@"SLBenchAxes"];
  NSMutableArray<NSString *> *list = [NSMutableArray new];
  for (NSString *axis in [(axes.length > 0 ? axes : @"y") componentsSeparatedByString:@","]) {
    NSString *trimmed = [axis stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
    if ([trimmed isEqualToString:@"y"] || [trimmed isEqualToString:@"x"] || [trimmed isEqualToString:@"xy"]) {
      [list addObject:trimmed];
    }
  }
  _axes = list.count > 0 ? list : @[ @"y" ];
  _speed = [ShadowListKitBench setting:@"SLBenchSpeed" fallback:4000];
  _seconds = [ShadowListKitBench setting:@"SLBenchSeconds" fallback:6];
  _nominal = 1.0 / UIScreen.mainScreen.maximumFramesPerSecond;
  _run = 0;
  [self startRun];
}

- (void)startRun
{
  _axis = _axes[_run];
  _drivesX = [_axis containsString:@"x"];
  _drivesY = [_axis containsString:@"y"];
  _vertical = _drivesY ? [ShadowListKitBench findScrollViewAlongX:NO] : nil;
  _horizontal = _drivesX ? [ShadowListKitBench findScrollViewAlongX:YES] : nil;
  if ((_drivesY && !_vertical) || (_drivesX && !_horizontal)) {
    [self skipRun:_drivesY && !_vertical ? @"no vertical scroll view" : @"no horizontal scroll view"];
    return;
  }
  // Rows are counted along y when a vertical view is driven or found, along x on a horizontal list.
  _coverage = _vertical ?: [ShadowListKitBench findScrollViewAlongX:NO] ?: _horizontal;
  NSMutableArray<UIView *> *driven = [NSMutableArray new];
  if (_vertical) {
    [driven addObject:_vertical];
  }
  if (_horizontal) {
    [driven addObject:_horizontal];
  }
  _probe = [ShadowListKitBench findProbeFrom:driven];
  _positionX = _horizontal.contentOffset.x;
  _positionY = _vertical.contentOffset.y;
  _directionX = _horizontal ? [self directionOf:_horizontal alongX:YES] : 1;
  _directionY = _vertical ? [self directionOf:_vertical alongX:NO] : 1;
  _last = 0;
  _legs = 0;
  _frame = 0;
  _travel = 0;
  _blankSum = 0;
  _blankMax = 0;
  _blankSamples = 0;
  _blankFrames = 0;
  _contentBlankSum = 0;
  _contentBlankMax = 0;
  _contentBlankFrames = 0;
  _intervals = [NSMutableArray arrayWithCapacity:2048];
  _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
  if (@available(iOS 15.0, *)) {
    float maxRate = (float)UIScreen.mainScreen.maximumFramesPerSecond;
    _link.preferredFrameRateRange = CAFrameRateRangeMake(maxRate, maxRate, maxRate);
  }
  _mainStart = ShadowListKitBenchThreadSeconds(ShadowListKitBenchMainThread);
  _processStart = ShadowListKitBenchProcessSeconds();
  _peakMB = ShadowListKitBenchFootprintMB();
  Class textView = NSClassFromString(@"ShadowListKitTextView");
  SEL reset = NSSelectorFromString(@"shadowListKit_resetAsyncStats");
  if ([textView respondsToSelector:reset]) {
    ((void (*)(id, SEL))[textView methodForSelector:reset])(textView, reset);
  }
  [_link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

/*
 * Move one axis a frame's distance and keep its position and direction. Returns whether it
 * stopped at an edge.
 */
- (BOOL)step:(UIScrollView *)scrollView
      alongX:(BOOL)alongX
    position:(double *)position
   direction:(int)direction
    interval:(CFTimeInterval)interval
{
  // The list may have moved its offset in its own layout since the last frame, like a
  // correction or an item window move. Continue from there.
  CGFloat current = alongX ? scrollView.contentOffset.x : scrollView.contentOffset.y;
  if (fabs(current - *position) >= 1) {
    *position = current;
  }
  double next = *position + _speed * interval * direction;
  CGFloat low = [self minOffsetOf:scrollView alongX:alongX];
  CGFloat high = [self maxOffsetOf:scrollView alongX:alongX];
  BOOL edge = NO;
  if (next <= low) {
    next = low;
    edge = YES;
  } else if (next >= high) {
    next = high;
    edge = YES;
  }
  _travel += fabs(next - *position);
  CGPoint offset = scrollView.contentOffset;
  if (alongX) {
    offset.x = (CGFloat)next;
  } else {
    offset.y = (CGFloat)next;
  }
  [scrollView setContentOffset:offset animated:NO];
  *position = alongX ? scrollView.contentOffset.x : scrollView.contentOffset.y;
  return edge;
}

- (void)tick:(CADisplayLink *)link
{
  CFTimeInterval now = link.timestamp;
  if (_last == 0) {
    _last = now;
    _legStart = now;
    return;
  }
  CFTimeInterval interval = now - _last;
  _last = now;
  [_intervals addObject:@(interval)];
  ++_frame;

  if (_frame % 3 == 0) {
    [self sampleBlank];
  }
  if (_frame % 30 == 0) {
    _peakMB = MAX(_peakMB, ShadowListKitBenchFootprintMB());
  }

  // A leg ends when its time is up or every driven axis stopped at an edge.
  BOOL edge = YES;
  if (_drivesY) {
    edge = [self step:_vertical alongX:NO position:&_positionY direction:_directionY interval:interval] && edge;
  }
  if (_drivesX) {
    edge = [self step:_horizontal alongX:YES position:&_positionX direction:_directionX interval:interval] && edge;
  }

  if (edge || now - _legStart >= _seconds) {
    ++_legs;
    _directionX = -_directionX;
    _directionY = -_directionY;
    _legStart = now;
    if (_legs >= 2) {
      [self finish];
    }
  }
}

/*
 * How much of the viewport no row covers, along the coverage view's scroll axis. Rows are the
 * views one or two levels into the scroll view that are neither containers nor scroll
 * indicators. With a probe, the content blank is the larger of that and the probe's share.
 */
- (void)sampleBlank
{
  UIScrollView *scrollView = _coverage;
  BOOL alongX = scrollView == _horizontal && scrollView != _vertical &&
    scrollView.contentSize.height <= scrollView.bounds.size.height;
  CGRect visible = scrollView.bounds;
  UIEdgeInsets inset = scrollView.adjustedContentInset;
  if (alongX) {
    visible.origin.x += inset.left;
    visible.size.width -= inset.left + inset.right;
  } else {
    visible.origin.y += inset.top;
    visible.size.height -= inset.top + inset.bottom;
  }
  CGFloat visibleStart = alongX ? visible.origin.x : visible.origin.y;
  CGFloat visibleAlong = alongX ? visible.size.width : visible.size.height;
  if (visibleAlong <= 0) {
    return;
  }
  CGFloat contentAlong = alongX ? scrollView.contentSize.width : scrollView.contentSize.height;
  CGFloat rootCross = alongX ? scrollView.bounds.size.height : scrollView.bounds.size.width;
  NSMutableArray<NSValue *> *spans = [NSMutableArray new];
  void (^collect)(UIView *, UIView *, int) = nil;
  __block __weak void (^weakCollect)(UIView *, UIView *, int);
  collect = ^(UIView *view, UIView *root, int depth) {
    for (UIView *child in view.subviews) {
      if (child.hidden || child.alpha < 0.01 || [child isKindOfClass:[UIRefreshControl class]]) {
        continue;
      }
      CGRect frame = [view convertRect:child.frame toView:root];
      CGFloat along = alongX ? frame.size.width : frame.size.height;
      CGFloat cross = alongX ? frame.size.height : frame.size.width;
      CGFloat start = alongX ? frame.origin.x : frame.origin.y;
      BOOL container = along >= contentAlong * 0.9 && contentAlong > visibleAlong * 1.5;
      if (container) {
        if (depth < 2) {
          weakCollect(child, root, depth + 1);
        }
        continue;
      }
      if (cross < rootCross * 0.3 || along < 1) {
        continue;
      }
      CGFloat low = MAX(start, visibleStart);
      CGFloat high = MIN(start + along, visibleStart + visibleAlong);
      if (high > low) {
        [spans addObject:[NSValue valueWithCGPoint:CGPointMake(low, high)]];
      }
    }
  };
  weakCollect = collect;
  collect(scrollView, scrollView, 0);
  [spans sortUsingComparator:^NSComparisonResult(NSValue *a, NSValue *b) {
    CGFloat left = a.CGPointValue.x;
    CGFloat right = b.CGPointValue.x;
    return left < right ? NSOrderedAscending : left > right ? NSOrderedDescending : NSOrderedSame;
  }];
  CGFloat covered = 0;
  CGFloat end = visibleStart;
  for (NSValue *value in spans) {
    CGPoint span = value.CGPointValue;
    if (span.y <= end) {
      continue;
    }
    covered += span.y - MAX(span.x, end);
    end = span.y;
  }
  double blank = MAX(0, 1 - covered / visibleAlong);
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"SLBenchDebugBlank"] && _blankSamples % 20 == 0) {
    NSMutableString *tree = [NSMutableString new];
    for (UIView *child in scrollView.subviews) {
      [tree appendFormat:@" %@%@(%lu)", NSStringFromClass([child class]), NSStringFromCGRect(child.frame), (unsigned long)child.subviews.count];
    }
    NSLog(@"[SLBENCH-BLANK] visible=%@ spans=%lu covered=%.0f blank=%.3f tree=%@", NSStringFromCGRect(visible),
      (unsigned long)spans.count, covered, blank, tree);
  }
  _blankSum += blank;
  _blankMax = MAX(_blankMax, blank);
  ++_blankSamples;
  if (blank > SHADOWLIST_KIT_BENCH_BLANK_FRAME) {
    ++_blankFrames;
  }
  if (_probe) {
    double content = MAX(blank, MIN(1.0, MAX(0.0, [_probe shadowListKit_benchBlankFraction])));
    _contentBlankSum += content;
    _contentBlankMax = MAX(_contentBlankMax, content);
    if (content > SHADOWLIST_KIT_BENCH_BLANK_FRAME) {
      ++_contentBlankFrames;
    }
  }
}

- (NSString *)label
{
  return [[NSUserDefaults standardUserDefaults] stringForKey:@"SLBenchLabel"] ?: @"";
}

- (void)skipRun:(NSString *)reason
{
  [self emit:@{@"label" : [self label], @"axis" : _axis, @"speed" : @(_speed), @"skipped" : reason}];
  [self nextRun];
}

- (void)finish
{
  [_link invalidate];
  _link = nil;
  double mainSeconds = ShadowListKitBenchThreadSeconds(ShadowListKitBenchMainThread) - _mainStart;
  double processSeconds = ShadowListKitBenchProcessSeconds() - _processStart;

  NSArray<NSNumber *> *sorted = [_intervals sortedArrayUsingSelector:@selector(compare:)];
  double total = 0;
  double hitch = 0;
  NSUInteger dropped = 0;
  NSUInteger hitches = 0;
  for (NSNumber *number in _intervals) {
    double interval = number.doubleValue;
    total += interval;
    if (interval > _nominal * 1.5) {
      hitch += interval - _nominal;
      dropped += (NSUInteger)llround(interval / _nominal) - 1;
      ++hitches;
    }
  }
  double (^percentile)(double) = ^double(double p) {
    if (sorted.count == 0) {
      return 0;
    }
    NSUInteger index = MIN(sorted.count - 1, (NSUInteger)(p * (sorted.count - 1)));
    return sorted[index].doubleValue * 1000;
  };
  UIScrollView *primary = _vertical ?: _horizontal;
  NSMutableDictionary *result = [@{
    @"label" : [self label],
    @"axis" : _axis,
    @"scrollView" : NSStringFromClass([primary class]),
    @"hz" : @(llround(1.0 / _nominal)),
    @"speed" : @(_speed),
    @"seconds" : @(total),
    @"travel" : @(llround(_travel)),
    @"frames" : @(_intervals.count),
    @"expectedFrames" : @(llround(total / _nominal)),
    @"dropped" : @(dropped),
    @"hitches" : @(hitches),
    @"hitchMsPerS" : @(total > 0 ? hitch * 1000 / total : 0),
    @"p50" : @(percentile(0.5)),
    @"p95" : @(percentile(0.95)),
    @"p99" : @(percentile(0.99)),
    @"max" : @(percentile(1.0)),
    @"mainCpuMsPerS" : @(total > 0 ? mainSeconds * 1000 / total : 0),
    @"processCpuMsPerS" : @(total > 0 ? processSeconds * 1000 / total : 0),
    @"mainCpuUsPerFrame" : @(_intervals.count > 0 ? mainSeconds * 1e6 / _intervals.count : 0),
    @"footprintMB" : @(ShadowListKitBenchFootprintMB()),
    @"peakMB" : @(_peakMB),
    @"blankAvg" : @(_blankSamples > 0 ? _blankSum / _blankSamples : 0),
    @"blankMax" : @(_blankMax),
    @"blankFrames" : @(_blankFrames),
    @"blankSamples" : @(_blankSamples),
  } mutableCopy];
  if (_vertical && _horizontal && _vertical != _horizontal) {
    result[@"horizontalScrollView"] = NSStringFromClass([_horizontal class]);
  }
  if (_probe) {
    result[@"probe"] = NSStringFromClass([_probe class]);
    result[@"contentBlankAvg"] = @(_blankSamples > 0 ? _contentBlankSum / _blankSamples : 0);
    result[@"contentBlankMax"] = @(_contentBlankMax);
    result[@"contentBlankFrames"] = @(_contentBlankFrames);
  }
  Class textView = NSClassFromString(@"ShadowListKitTextView");
  SEL stats = NSSelectorFromString(@"shadowListKit_asyncStats");
  if ([textView respondsToSelector:stats]) {
    NSDictionary *asyncText = ((NSDictionary * (*)(id, SEL))[textView methodForSelector:stats])(textView, stats);
    if (asyncText) {
      result[@"asyncText"] = asyncText;
    }
  }
  [self emit:result];
  [self nextRun];
}

/*
 * Log one result line. The file keeps the last one.
 */
- (void)emit:(NSDictionary *)result
{
  if (![NSJSONSerialization isValidJSONObject:result]) {
    NSLog(@"[SLBENCH] result is not valid JSON: %@", result);
    return;
  }
  NSData *json = [NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingSortedKeys error:nil];
  NSString *line = [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
  NSLog(@"[SLBENCH] %@", line);
  printf("[SLBENCH] %s\n", line.UTF8String);
  fflush(stdout);
  NSURL *documents = [NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
  [json writeToURL:[documents URLByAppendingPathComponent:@"slbench.json"] atomically:YES];
}

- (void)nextRun
{
  ++_run;
  if (_run < _axes.count) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(SHADOWLIST_KIT_BENCH_RUN_GAP * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      [self startRun];
    });
    return;
  }
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"SLBenchExit"]) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      exit(0);
    });
  }
}

@end
