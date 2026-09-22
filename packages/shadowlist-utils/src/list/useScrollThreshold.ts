import { useCallback, useMemo, useRef, useState } from 'react';

export interface ScrollOffsetEvent {
  nativeEvent: {
    contentOffsetY: number;
  };
}

export interface ScrollThreshold {
  isPastThreshold: boolean;
  onScroll: (event: ScrollOffsetEvent) => void;
}

/**
 * Tracks whether a list has scrolled past `thresholdDp`. Re-renders only when that flips,
 * not on every scroll event. Use it to hide a sticky header, show a back to top button, or
 * collapse a toolbar.
 *
 * @example
 * const { isPastThreshold, onScroll } = useScrollThreshold(220);
 * <ShadowList stickyHeader={!isPastThreshold} onScroll={onScroll} ... />
 */
export function useScrollThreshold(thresholdDp: number): ScrollThreshold {
  const thresholdRef = useRef(thresholdDp);
  thresholdRef.current = thresholdDp;

  const [isPastThreshold, setIsPastThreshold] = useState(false);
  const isPastRef = useRef(false);

  const onScroll = useCallback((event: ScrollOffsetEvent) => {
    const isPast = event.nativeEvent.contentOffsetY >= thresholdRef.current;
    if (isPast === isPastRef.current) return;
    isPastRef.current = isPast;
    setIsPastThreshold(isPast);
  }, []);

  return useMemo(
    () => ({ isPastThreshold, onScroll }),
    [isPastThreshold, onScroll]
  );
}
