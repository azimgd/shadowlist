#import <Foundation/Foundation.h>

/*
 * A scroll benchmark that works on any app with a vertical UIScrollView. It starts on its own
 * when the app is launched with -SLBench 1, drives the largest scroll view at a constant
 * speed and logs one [SLBENCH] JSON line with frame, CPU, memory and blank area numbers.
 * Built into the UIKit example, and injected into other apps as a dylib.
 *
 * Launch arguments:
 *   -SLBenchSpeed 4000     points per second
 *   -SLBenchSeconds 6      seconds per leg, the run goes away from where it opened and back
 *   -SLBenchDelay 3        seconds to wait for the list to load first
 *   -SLBenchLabel name     copied into the result
 *   -SLBenchExit 1         quit when done, for a script waiting on the process
 */
@interface SLKBench : NSObject
@end
