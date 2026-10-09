#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * A scroll benchmark that works on any app with a UIScrollView. It starts on its own when the
 * app is launched with -SLBench 1, drives the largest scroll view at a constant speed and logs
 * one [SLBENCH] JSON line per axis with frame, CPU, memory and blank area numbers.
 * Built into the UIKit example, and injected into other apps as a dylib.
 *
 * Launch arguments:
 *   -SLBenchAxes y         axes to run in order, comma separated: y, x or xy. Default y.
 *                          y drives the vertical scroll view with the most content, x the
 *                          horizontal one, xy both at once, one view or two. An axis with no
 *                          scroll view logs a line with skipped set.
 *   -SLBenchSpeed 4000     points per second, on each driven axis
 *   -SLBenchSeconds 6      seconds per leg, the run goes away from where it opened and back
 *   -SLBenchDelay 3        seconds to wait for the list to load first
 *   -SLBenchLabel name     copied into the result
 *   -SLBenchExit 1         quit when done, for a script waiting on the process
 */
@interface SLKBench : NSObject
@end

/*
 * A view that draws its content after its rows show, like tiles drawn in the background, can
 * tell the bench how much of its viewport is not drawn yet. The bench asks the driven scroll
 * views and their superviews. The first one that answers is the probe. Results then carry
 * contentBlankAvg, contentBlankMax and contentBlankFrames, where a frame's blank is the larger
 * of the uncovered share and the probe's. blankAvg, blankMax and blankFrames stay the uncovered
 * share alone.
 */
@protocol SLKBenchProbe <NSObject>

/*
 * The share of the viewport, from 0 to 1, with content not drawn yet.
 */
- (double)slk_benchBlankFraction;

@end

NS_ASSUME_NONNULL_END
