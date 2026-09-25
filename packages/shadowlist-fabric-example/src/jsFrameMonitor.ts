import { launchSetting } from './launchSettings';

/*
 * BENCHMARK-ONLY. With SLJsFps 1, counts JS thread frames with requestAnimationFrame and logs
 * one [SLFPS] line per second while frames are late: frames, frames over 20 ms, the longest gap.
 * It measures any list the same way, with or without the native [SLC] trace.
 */
/*
 * console.log reaches logcat (ReactNativeJS) on Android. On iOS the app delegate lowers the
 * log threshold and copies [SLFPS] lines to stderr when SLJsFps is on, Release included.
 */
const log = (message: string) => console.log(message);

if (launchSetting('SLJsFps') === '1') {
  let last = 0;
  let windowStart = 0;
  let frames = 0;
  let slow = 0;
  let longest = 0;
  const tick = (now: number) => {
    if (last !== 0) {
      const gap = now - last;
      frames++;
      if (gap > 20) slow++;
      if (gap > longest) longest = gap;
    } else {
      windowStart = now;
    }
    last = now;
    if (now - windowStart >= 1000) {
      log(
        `[SLFPS] t=${Math.round(now)} span=${Math.round(now - windowStart)}` +
          ` frames=${frames} slow=${slow} longest=${longest.toFixed(1)}`
      );
      windowStart = now;
      frames = 0;
      slow = 0;
      longest = 0;
    }
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
}
