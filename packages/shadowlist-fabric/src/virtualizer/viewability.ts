import type { ViewabilityConfig } from '../types';

/*
 * The rules native reports viewable rows for, two numbers each: the threshold from 0 to 1,
 * then 1 for viewport coverage or 0 for the row's own share. The first rule is any overlap,
 * which picks the pinned section header. One rule follows for each config.
 */
export function viewableRulesFor(
  configs: ReadonlyArray<ViewabilityConfig>
): number[] {
  const rules = [0, 0];
  for (const config of configs) {
    const coverage = config.viewAreaCoveragePercentThreshold != null;
    const percent = coverage
      ? config.viewAreaCoveragePercentThreshold!
      : (config.itemVisiblePercentThreshold ?? 0);
    rules.push(Math.min(100, Math.max(0, percent)) / 100, coverage ? 1 : 0);
  }
  return rules;
}

/*
 * When native reports each config, two numbers each: minimumViewTime in milliseconds, then 1
 * for waitForInteraction. Native holds a report until the rows stayed viewable that long, and
 * reports nothing before recordInteraction.
 */
export function viewableTimingsFor(
  configs: ReadonlyArray<ViewabilityConfig>
): number[] {
  const timings: number[] = [];
  for (const config of configs) {
    timings.push(
      Math.max(0, config.minimumViewTime ?? 0),
      config.waitForInteraction ? 1 : 0
    );
  }
  return timings;
}
