// Number of extra rows mounted on each side of the visible window
export const SHADOWLIST_OVERSCAN = 4;

/*
 * Extra rows mounted ahead of the visible window, in the direction the list is actually
 * travelling. A blank cell during a fling is a row the native side has already scrolled
 * to but React has not mounted yet, so what protects against it is runway in front of the
 * user -- not a bigger buffer behind them, which only costs render work. This is the
 * leading side's total, so a fling mounts SHADOWLIST_OVERSCAN_LEADING rows ahead and
 * SHADOWLIST_OVERSCAN behind; a resting list keeps SHADOWLIST_OVERSCAN on both sides.
 */
export const SHADOWLIST_OVERSCAN_LEADING = 10;

// Maps the public snapToAlignment prop to the native enum value
export const SNAP_ALIGNMENT = { start: 0, center: 1, end: 2 } as const;

const SHADOWLIST_DEBUG_LOG = false;

export function slLog(...args: unknown[]): void {
  if (!SHADOWLIST_DEBUG_LOG) return;
  console.log('[SL]', ...args);
}

// Move the element at `from` to `to`, returning a new array.
export function arrayMove<ElementT>(
  input: ReadonlyArray<ElementT>,
  from: number,
  to: number
): ElementT[] {
  const next = input.slice();
  if (
    from < 0 ||
    from >= next.length ||
    to < 0 ||
    to >= next.length ||
    from === to
  ) {
    return next;
  }
  const moved = next.splice(from, 1)[0] as ElementT;
  next.splice(to, 0, moved);
  return next;
}
