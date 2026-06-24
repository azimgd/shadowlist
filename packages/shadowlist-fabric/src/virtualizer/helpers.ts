// Number of extra rows mounted on each side of the visible window
export const SHADOWLIST_OVERSCAN = 4;

// Maps the public snapToAlignment prop to the native enum value
export const SNAP_ALIGNMENT = { start: 0, center: 1, end: 2 } as const;

const SHADOWLIST_DEBUG_LOG = false;

export const slLog = (...args: unknown[]) => {
  if (!SHADOWLIST_DEBUG_LOG) return;
  console.log('[SL]', ...args);
};

// Move the item at `from` to `to`, returning a new array.
export const arrayMove = <T>(
  input: ReadonlyArray<T>,
  from: number,
  to: number
): T[] => {
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
  const moved = next.splice(from, 1)[0] as T;
  next.splice(to, 0, moved);
  return next;
};
