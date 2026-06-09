import type { MountedRange } from './mountedRange';

/*
 * Runtime overscan as a fraction of the visible range (which is ~one viewport), biased
 * toward the scroll direction so the prerender band leads ahead of travel and trails
 * less behind. The band is mounted as a low-priority transition by the caller.
 */
export const SHADOWLIST_OVERSCAN_LEADING_RATIO = 1.5;
export const SHADOWLIST_OVERSCAN_TRAILING_RATIO = 0.5;

/*
 * Re-band hysteresis: while the visible range is fully mounted, only extend once the
 * mounted lead ahead of travel falls below this fraction of the leading overscan.
 * Between re-bands visible-indices events are no-ops.
 */
export const SHADOWLIST_EXTEND_AT_LEAD_RATIO = 0.5;

export interface ResolveVisibleIndicesInput {
  /* Raw native visible range; inverted lists report start > end, -1 when uninitialized. */
  visibleStartIndex: number;
  visibleEndIndex: number;
  /* Previous normalized visible range ({low: -1, high: -1} before the first event). */
  prevVisibleRange: MountedRange;
  /* Latest scheduled mounted range ({low: -1, high: -1} when nothing is mounted). */
  mountedRange: MountedRange;
  dataLength: number;
}

export type ResolveVisibleIndicesResult =
  /* Uninitialized event or empty list: nothing to do, previous visible range stands. */
  | { action: 'ignore' }
  /* Visible range comfortably inside the mounted range: no state update at all. */
  | { action: 'skip'; visibleRange: MountedRange }
  /* Hysteresis tripped: extend ahead of travel and trim the trail as a transition. */
  | { action: 'reband'; visibleRange: MountedRange; band: MountedRange }
  /* Visible range left the mounted rows (fling / scrollToIndex): mount the band. */
  | { action: 'mount'; visibleRange: MountedRange; band: MountedRange };

/*
 * Decide how the mounted range reacts to a native visible-indices event. Pure: the
 * caller owns the refs (previous visible range, scheduled range) and the React state
 * updates; this only encodes the policy. A slow scroll resolves to `skip` for most
 * events and one `reband` per ~viewport of travel; a fling resolves to `mount`.
 */
export const resolveVisibleIndices = (
  input: ResolveVisibleIndicesInput
): ResolveVisibleIndicesResult => {
  const {
    visibleStartIndex,
    visibleEndIndex,
    prevVisibleRange,
    mountedRange,
    dataLength,
  } = input;

  if (visibleStartIndex === -1 || visibleEndIndex === -1 || dataLength <= 0) {
    return { action: 'ignore' };
  }

  // Normalise to an ascending range (inverted lists report start > end).
  const visibleLow = Math.min(visibleStartIndex, visibleEndIndex);
  const visibleHigh = Math.max(visibleStartIndex, visibleEndIndex);
  const visibleRange = { low: visibleLow, high: visibleHigh };

  // Bias the overscan toward the scroll direction: lead ahead of travel, trail less
  // behind. The visible span is ~one viewport, so the band is expressed relative to it.
  const scrollingForward =
    prevVisibleRange.low < 0 || visibleLow >= prevVisibleRange.low;

  const maxIndex = dataLength - 1;
  const visibleSpan = visibleHigh - visibleLow + 1;
  const overscanLeading = Math.ceil(
    visibleSpan * SHADOWLIST_OVERSCAN_LEADING_RATIO
  );
  const overscanTrailing = Math.ceil(
    visibleSpan * SHADOWLIST_OVERSCAN_TRAILING_RATIO
  );
  const band = {
    low: Math.max(
      0,
      visibleLow - (scrollingForward ? overscanTrailing : overscanLeading)
    ),
    high: Math.min(
      maxIndex,
      visibleHigh + (scrollingForward ? overscanLeading : overscanTrailing)
    ),
  };

  const visibleRangeMounted =
    mountedRange.low >= 0 &&
    visibleLow >= mountedRange.low &&
    visibleHigh <= mountedRange.high;

  if (!visibleRangeMounted) {
    return { action: 'mount', visibleRange, band };
  }

  /*
   * Visible range is fully mounted: apply hysteresis instead of chasing the exact band
   * on every row crossing. Extend only when the mounted lead ahead of travel drops
   * below half the leading overscan (clamped to what the list edge allows); trim only
   * when the trail behind outgrows the trailing overscan by more than one visible
   * span.
   */
  const leadAhead = scrollingForward
    ? mountedRange.high - visibleHigh
    : visibleLow - mountedRange.low;
  const leadTarget = scrollingForward
    ? band.high - visibleHigh
    : visibleLow - band.low;
  const minLead = Math.min(
    Math.ceil(overscanLeading * SHADOWLIST_EXTEND_AT_LEAD_RATIO),
    leadTarget
  );
  const trailBehind = scrollingForward
    ? visibleLow - mountedRange.low
    : mountedRange.high - visibleHigh;
  const maxTrail = overscanTrailing + visibleSpan;

  if (leadAhead >= minLead && trailBehind <= maxTrail) {
    return { action: 'skip', visibleRange };
  }

  return { action: 'reband', visibleRange, band };
};
