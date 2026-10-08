import type { MountedRange } from './mountedRange';

/*
 * Rows to prefetch ahead of the mounted ones, the same rules as the native lists' prefetching
 * in host/ListDriver.cpp. Rows of the window that are not mounted get prefetched once. A
 * prefetched row that leaves the window before it was mounted is cancelled. Fabric mounts rows
 * from JS, which is why the rules run here. Rows are held by key and reported by index, low to
 * high.
 */
export class PrefetchTracker {
  private prefetched = new Set<string>();

  update(
    keys: ReadonlyArray<string>,
    keyToIndex: ReadonlyMap<string, number>,
    mounted: MountedRange | null,
    window: MountedRange | null
  ): { prefetch: number[]; cancel: number[] } {
    const prefetch: number[] = [];
    const cancel: number[] = [];
    const isMounted = (index: number) =>
      mounted !== null && index >= mounted.low && index <= mounted.high;
    const inWindow = (index: number) =>
      window !== null && index >= window.low && index <= window.high;
    // Rows prefetched earlier: shown now, gone, or out of the window.
    for (const key of [...this.prefetched]) {
      const index = keyToIndex.get(key);
      if (index === undefined || isMounted(index)) {
        this.prefetched.delete(key);
      } else if (!inWindow(index)) {
        cancel.push(index);
        this.prefetched.delete(key);
      }
    }
    if (window !== null) {
      const high = Math.min(window.high, keys.length - 1);
      for (let index = Math.max(0, window.low); index <= high; index++) {
        const key = keys[index]!;
        if (!isMounted(index) && !this.prefetched.has(key)) {
          this.prefetched.add(key);
          prefetch.push(index);
        }
      }
    }
    cancel.sort((a, b) => a - b);
    return { prefetch, cancel };
  }
}

/*
 * The window to prefetch: the mounted rows plus rows on each side.
 */
export function prefetchWindow(
  mounted: MountedRange | null,
  rows: number,
  count: number
): MountedRange | null {
  if (mounted === null || count === 0 || rows <= 0) return null;
  return {
    low: Math.max(0, mounted.low - rows),
    high: Math.min(count - 1, mounted.high + rows),
  };
}
