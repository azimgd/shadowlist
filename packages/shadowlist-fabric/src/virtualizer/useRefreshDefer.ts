import {
  useCallback,
  useEffect,
  useLayoutEffect,
  useReducer,
  useRef,
} from 'react';
import { slTrace, slTraceEnabled } from './helpers';

interface UseRefreshDeferOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  refreshing: boolean;
  onRefresh: (() => void) | undefined;
  inverted: boolean;
  horizontal: boolean;
}

interface UseRefreshDeferResult<ElementT> {
  data: ReadonlyArray<ElementT>;
  handleRefreshSettle: () => void;
}

/*
 * Hold data that arrives during a refresh until the spinner is fully gone, then apply it as
 * a normal prepend so the visible content stays in place. Other changes pass straight through.
 * Only for vertical, non inverted lists with onRefresh.
 */
export function useRefreshDefer<ElementT>({
  data: dataProp,
  refreshing,
  onRefresh,
  inverted,
  horizontal,
}: UseRefreshDeferOptions<ElementT>): UseRefreshDeferResult<ElementT> {
  const refreshDeferEnabled = !!onRefresh && !inverted && !horizontal;

  const refreshHoldingRef = useRef(false);
  const refreshHeldDataRef = useRef<ReadonlyArray<ElementT> | null>(null);
  const previousRefreshingRef = useRef(refreshing);
  /*
   * The data the last commit showed. Kept in a ref, not state, so a normal data change
   * renders once. Setting state here during render made React run the whole list twice.
   */
  const shownDataRef = useRef(dataProp);
  const [, forceRender] = useReducer((count: number) => count + 1, 0);

  if (refreshDeferEnabled && !previousRefreshingRef.current && refreshing) {
    // A refresh just started. Hold data changes until it settles.
    refreshHoldingRef.current = true;
  }
  previousRefreshingRef.current = refreshing;

  let data = dataProp;
  if (refreshHoldingRef.current && dataProp !== shownDataRef.current) {
    if (slTraceEnabled() && refreshHeldDataRef.current !== dataProp) {
      slTrace(`refresh hold n=${dataProp.length}`);
    }
    refreshHeldDataRef.current = dataProp;
    data = shownDataRef.current;
  }

  useLayoutEffect(() => {
    shownDataRef.current = data;
  });

  // Apply the held data once native says the spinner is gone.
  const handleRefreshSettle = useCallback(() => {
    if (slTraceEnabled()) {
      slTrace(
        `refresh settle held=${refreshHeldDataRef.current !== null ? 1 : 0}`
      );
    }
    refreshHoldingRef.current = false;
    if (refreshHeldDataRef.current !== null) {
      /*
       * Render again with holding off, which shows the newest data prop. That is the held
       * data, or something newer the caller passed since.
       */
      refreshHeldDataRef.current = null;
      forceRender();
    }
  }, []);

  /*
   * Fallback in case onRefreshSettle never comes. Release the held data shortly after the
   * refresh ends. On iOS the settle event fires first, so this does nothing there.
   */
  const previousRefreshingForFallbackRef = useRef(refreshing);
  useEffect(() => {
    const wasRefreshing = previousRefreshingForFallbackRef.current;
    previousRefreshingForFallbackRef.current = refreshing;
    if (refreshDeferEnabled && wasRefreshing && !refreshing) {
      const timer = setTimeout(handleRefreshSettle, 1200);
      return () => clearTimeout(timer);
    }
    return undefined;
  }, [refreshing, refreshDeferEnabled, handleRefreshSettle]);

  return { data, handleRefreshSettle };
}
