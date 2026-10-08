#import "SLKBench.h"

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <mach/mach.h>
#import <sys/resource.h>

static thread_act_t SLKBenchMainThread;

static double SLKBenchThreadSeconds(thread_act_t thread)
{
  thread_basic_info_data_t info;
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  if (thread_info(thread, THREAD_BASIC_INFO, (thread_info_t)&info, &count) != KERN_SUCCESS) {
    return 0;
  }
  return info.user_time.seconds + info.user_time.microseconds / 1e6 + info.system_time.seconds + info.system_time.microseconds / 1e6;
}

static double SLKBenchProcessSeconds(void)
{
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
}

static double SLKBenchFootprintMB(void)
{
  task_vm_info_data_t info;
  mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) {
    return 0;
  }
  return info.phys_footprint / 1048576.0;
}

@implementation SLKBench {
  UIScrollView *_scrollView;
  CADisplayLink *_link;
  CFTimeInterval _last;
  CFTimeInterval _legStart;
  double _speed;
  double _seconds;
  int _direction;
  int _legs;
  double _position;
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
  double _travel;
}

+ (void)load
{
  SLKBenchMainThread = mach_thread_self();
  if (![[NSUserDefaults standardUserDefaults] stringForKey:@"SLBench"]) {
    return;
  }
  static SLKBench *bench;
  [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification
                                                    object:nil
                                                     queue:NSOperationQueue.mainQueue
                                                usingBlock:^(NSNotification *note) {
                                                  if (bench) {
                                                    return;
                                                  }
                                                  bench = [SLKBench new];
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
 * The vertical scroll view with the most content, wider than half the screen.
 */
+ (UIScrollView *)findScrollView
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
    CGFloat range = scrollView.contentSize.height - scrollView.bounds.size.height;
    if (scrollView.bounds.size.width > scrollView.window.bounds.size.width * 0.5 && range > bestRange) {
      best = scrollView;
      bestRange = range;
    }
  }
  return best;
}

- (CGFloat)minOffset
{
  return -_scrollView.adjustedContentInset.top;
}

- (CGFloat)maxOffset
{
  UIEdgeInsets inset = _scrollView.adjustedContentInset;
  return MAX(-inset.top, _scrollView.contentSize.height + inset.bottom - _scrollView.bounds.size.height);
}

- (void)start
{
  _scrollView = [SLKBench findScrollView];
  if (!_scrollView) {
    NSLog(@"[SLBENCH] {\"error\":\"no scroll view\"}");
    return;
  }
  _speed = [SLKBench setting:@"SLBenchSpeed" fallback:4000];
  _seconds = [SLKBench setting:@"SLBenchSeconds" fallback:6];
  // Go away from the edge the list opened at: down from the top, up from the bottom.
  CGFloat offset = _scrollView.contentOffset.y;
  _direction = offset > ([self minOffset] + [self maxOffset]) / 2 ? -1 : 1;
  _position = offset;
  _intervals = [NSMutableArray arrayWithCapacity:2048];
  _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
  if (@available(iOS 15.0, *)) {
    float maxRate = (float)UIScreen.mainScreen.maximumFramesPerSecond;
    _link.preferredFrameRateRange = CAFrameRateRangeMake(maxRate, maxRate, maxRate);
  }
  _nominal = 1.0 / UIScreen.mainScreen.maximumFramesPerSecond;
  _mainStart = SLKBenchThreadSeconds(SLKBenchMainThread);
  _processStart = SLKBenchProcessSeconds();
  _peakMB = SLKBenchFootprintMB();
  Class textView = NSClassFromString(@"SLKTextView");
  if ([textView respondsToSelector:@selector(slk_resetAsyncStats)]) {
    [textView performSelector:@selector(slk_resetAsyncStats)];
  }
  [_link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
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
    _peakMB = MAX(_peakMB, SLKBenchFootprintMB());
  }

  double step = _speed * interval * _direction;
  double next = _position + step;
  CGFloat low = [self minOffset];
  CGFloat high = [self maxOffset];
  BOOL edge = NO;
  if (next <= low) {
    next = low;
    edge = YES;
  } else if (next >= high) {
    next = high;
    edge = YES;
  }
  _travel += fabs(next - _position);
  _position = next;
  [_scrollView setContentOffset:CGPointMake(_scrollView.contentOffset.x, (CGFloat)next) animated:NO];
  // The list may have moved the offset itself, like a correction. Continue from there.
  _position = _scrollView.contentOffset.y;

  if (edge || now - _legStart >= _seconds) {
    ++_legs;
    _direction = -_direction;
    _legStart = now;
    if (_legs >= 2) {
      [self finish];
    }
  }
}

/*
 * How much of the viewport no row covers. Rows are the views one or two levels into the
 * scroll view that are neither containers nor scroll indicators.
 */
- (void)sampleBlank
{
  CGRect visible = _scrollView.bounds;
  UIEdgeInsets inset = _scrollView.adjustedContentInset;
  visible.origin.y += inset.top;
  visible.size.height -= inset.top + inset.bottom;
  if (visible.size.height <= 0) {
    return;
  }
  CGFloat contentHeight = _scrollView.contentSize.height;
  NSMutableArray<NSValue *> *spans = [NSMutableArray new];
  void (^collect)(UIView *, UIView *, int) = nil;
  __block __weak void (^weakCollect)(UIView *, UIView *, int);
  collect = ^(UIView *view, UIView *root, int depth) {
    for (UIView *child in view.subviews) {
      if (child.hidden || child.alpha < 0.01 || [child isKindOfClass:[UIRefreshControl class]]) {
        continue;
      }
      CGRect frame = [view convertRect:child.frame toView:root];
      BOOL container = frame.size.height >= contentHeight * 0.9 && contentHeight > visible.size.height * 1.5;
      if (container) {
        if (depth < 2) {
          weakCollect(child, root, depth + 1);
        }
        continue;
      }
      if (frame.size.width < root.bounds.size.width * 0.3 || frame.size.height < 1) {
        continue;
      }
      CGFloat top = MAX(frame.origin.y, visible.origin.y);
      CGFloat bottom = MIN(CGRectGetMaxY(frame), CGRectGetMaxY(visible));
      if (bottom > top) {
        [spans addObject:[NSValue valueWithCGPoint:CGPointMake(top, bottom)]];
      }
    }
  };
  weakCollect = collect;
  collect(_scrollView, _scrollView, 0);
  [spans sortUsingComparator:^NSComparisonResult(NSValue *a, NSValue *b) {
    return a.CGPointValue.x < b.CGPointValue.x ? NSOrderedAscending : NSOrderedDescending;
  }];
  CGFloat covered = 0;
  CGFloat end = visible.origin.y;
  for (NSValue *value in spans) {
    CGPoint span = value.CGPointValue;
    if (span.y <= end) {
      continue;
    }
    covered += span.y - MAX(span.x, end);
    end = span.y;
  }
  double blank = MAX(0, 1 - covered / visible.size.height);
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"SLBenchDebugBlank"] && _blankSamples % 20 == 0) {
    NSMutableString *tree = [NSMutableString new];
    for (UIView *child in _scrollView.subviews) {
      [tree appendFormat:@" %@%@(%lu)", NSStringFromClass([child class]), NSStringFromCGRect(child.frame), (unsigned long)child.subviews.count];
    }
    NSLog(@"[SLBENCH-BLANK] visible=%@ spans=%lu covered=%.0f blank=%.3f tree=%@", NSStringFromCGRect(visible),
      (unsigned long)spans.count, covered, blank, tree);
  }
  _blankSum += blank;
  _blankMax = MAX(_blankMax, blank);
  ++_blankSamples;
  if (blank > 0.02) {
    ++_blankFrames;
  }
}

- (void)finish
{
  [_link invalidate];
  _link = nil;
  double mainSeconds = SLKBenchThreadSeconds(SLKBenchMainThread) - _mainStart;
  double processSeconds = SLKBenchProcessSeconds() - _processStart;

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
  NSDictionary *result = @{
    @"label" : [[NSUserDefaults standardUserDefaults] stringForKey:@"SLBenchLabel"] ?: @"",
    @"scrollView" : NSStringFromClass([_scrollView class]),
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
    @"footprintMB" : @(SLKBenchFootprintMB()),
    @"peakMB" : @(_peakMB),
    @"blankAvg" : @(_blankSamples > 0 ? _blankSum / _blankSamples : 0),
    @"blankMax" : @(_blankMax),
    @"blankFrames" : @(_blankFrames),
    @"blankSamples" : @(_blankSamples),
  };
  Class textView = NSClassFromString(@"SLKTextView");
  if ([textView respondsToSelector:@selector(slk_asyncStats)]) {
    NSMutableDictionary *withText = [result mutableCopy];
    withText[@"asyncText"] = [textView performSelector:@selector(slk_asyncStats)];
    result = withText;
  }
  NSData *json = [NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingSortedKeys error:nil];
  NSString *line = [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
  NSLog(@"[SLBENCH] %@", line);
  printf("[SLBENCH] %s\n", line.UTF8String);
  fflush(stdout);
  NSURL *documents = [NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
  [json writeToURL:[documents URLByAppendingPathComponent:@"slbench.json"] atomically:YES];
  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"SLBenchExit"]) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      exit(0);
    });
  }
}

@end
