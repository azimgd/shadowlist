import { useReducer, useRef, useCallback, useMemo, useEffect } from 'react';

/* This package's tsconfig targets ESNext with no DOM or Node lib, so the timer
 * globals aren't typed. Read them off globalThis with a minimal shape, kept on the
 * object (not destructured) so the calls stay bound to the global. */
const timers = globalThis as unknown as {
  setTimeout: (handler: () => void, timeout: number) => number;
  clearTimeout: (handle: number) => void;
};

/*
 * Centralises the common UI state every list screen ends up re-implementing:
 * the refreshing / loadingMore / loadingOlder flags, the re-entrancy guards
 * around them, and the data array itself (prepend / append / remove). Connect its
 * `handle*` callbacks straight into the list; each one flips its flag
 * automatically while your async work runs, and never double-fires.
 */

export interface UseListControllerOptions<
  ItemT,
  ScrollEventT = unknown,
  ViewableInfoT = unknown,
> {
  /* Seed rows; the controller owns the array from here on. */
  initialData?: readonly ItemT[];
  /* Pull-to-refresh work. `refreshing` is true while it runs. */
  onRefresh?: () => void | Promise<void>;
  /* Load-more work for the end edge. `loadingMore` is true while it runs. */
  onEndReached?: () => void | Promise<void>;
  /* Load-older work for the start edge. `loadingOlder` is true while it runs. */
  onStartReached?: () => void | Promise<void>;
  /* Forwarded as-is; the controller only flips the `scrolling` flag around it. */
  onScroll?: (event: ScrollEventT) => void;
  /* Forwarded as-is. */
  onViewableItemsChanged?: (info: ViewableInfoT) => void;
  /* Idle delay (ms) before `scrolling` flips back to false. Default 150. */
  scrollIdleMs?: number;
}

interface ListState<ItemT> {
  data: ItemT[];
  refreshing: boolean;
  loadingMore: boolean;
  loadingOlder: boolean;
  scrolling: boolean;
}

/* The reducer actions, named as the start/end markers the state moves between. */
type ListAction<ItemT> =
  | { type: 'refreshStarted' }
  | { type: 'refreshEnded' }
  | { type: 'endReachStarted' }
  | { type: 'endReachEnded' }
  | { type: 'startReachStarted' }
  | { type: 'startReachEnded' }
  | { type: 'scrollStarted' }
  | { type: 'scrollEnded' }
  | { type: 'itemsSet'; update: ItemT[] | ((prev: ItemT[]) => ItemT[]) }
  | { type: 'itemsPrepended'; items: readonly ItemT[] }
  | { type: 'itemsAppended'; items: readonly ItemT[] }
  | { type: 'itemsRemoved'; match: (item: ItemT, index: number) => boolean };

/* Flag actions return the same state object when nothing changes, so an already-true
 * `scrollStarted` on every scroll event doesn't trigger a re-render. */
function listReducer<ItemT>(
  state: ListState<ItemT>,
  action: ListAction<ItemT>
): ListState<ItemT> {
  switch (action.type) {
    case 'refreshStarted':
      return state.refreshing ? state : { ...state, refreshing: true };
    case 'refreshEnded':
      return state.refreshing ? { ...state, refreshing: false } : state;
    case 'endReachStarted':
      return state.loadingMore ? state : { ...state, loadingMore: true };
    case 'endReachEnded':
      return state.loadingMore ? { ...state, loadingMore: false } : state;
    case 'startReachStarted':
      return state.loadingOlder ? state : { ...state, loadingOlder: true };
    case 'startReachEnded':
      return state.loadingOlder ? { ...state, loadingOlder: false } : state;
    case 'scrollStarted':
      return state.scrolling ? state : { ...state, scrolling: true };
    case 'scrollEnded':
      return state.scrolling ? { ...state, scrolling: false } : state;
    case 'itemsSet':
      return {
        ...state,
        data:
          typeof action.update === 'function'
            ? action.update(state.data)
            : action.update,
      };
    case 'itemsPrepended':
      return { ...state, data: [...action.items, ...state.data] };
    case 'itemsAppended':
      return { ...state, data: [...state.data, ...action.items] };
    case 'itemsRemoved':
      return {
        ...state,
        data: state.data.filter((item, index) => !action.match(item, index)),
      };
    default:
      return state;
  }
}

/* Raw markers: flip the state by hand when you aren't using the `handle*` wrappers. */
export interface ListMarkers {
  refreshStarted: () => void;
  refreshEnded: () => void;
  endReachStarted: () => void;
  endReachEnded: () => void;
  startReachStarted: () => void;
  startReachEnded: () => void;
  scrollStarted: () => void;
  scrollEnded: () => void;
}

export interface ListController<
  ItemT,
  ScrollEventT = unknown,
  ViewableInfoT = unknown,
> {
  data: ItemT[];
  refreshing: boolean;
  loadingMore: boolean;
  loadingOlder: boolean;
  scrolling: boolean;

  handleRefresh: () => void;
  handleEndReached: () => void;
  handleStartReached: () => void;
  handleScroll: (event: ScrollEventT) => void;
  handleViewableItemsChanged: (info: ViewableInfoT) => void;

  setData: (update: ItemT[] | ((prev: ItemT[]) => ItemT[])) => void;
  prepend: (items: readonly ItemT[]) => void;
  append: (items: readonly ItemT[]) => void;
  removeItems: (
    ids: readonly string[] | ((item: ItemT, index: number) => boolean)
  ) => void;

  markers: ListMarkers;
}

export function useListController<
  ItemT extends { id: string },
  ScrollEventT = unknown,
  ViewableInfoT = unknown,
>(
  options: UseListControllerOptions<ItemT, ScrollEventT, ViewableInfoT> = {}
): ListController<ItemT, ScrollEventT, ViewableInfoT> {
  const [state, dispatch] = useReducer(
    listReducer<ItemT>,
    options.initialData,
    (seed): ListState<ItemT> => ({
      data: seed ? [...seed] : [],
      refreshing: false,
      loadingMore: false,
      loadingOlder: false,
      scrolling: false,
    })
  );

  // Latest options in a ref so the returned handlers can stay referentially stable.
  const optionsRef = useRef(options);
  optionsRef.current = options;

  // Synchronous busy guards. dispatch only flips a flag on the next render, so a
  // second call in the same tick would slip past a state-based check.
  const busyRef = useRef({ refresh: false, end: false, start: false });

  const scrollIdleTimer = useRef<number | null>(null);

  const handleRefresh = useCallback(() => {
    if (busyRef.current.refresh) return;
    busyRef.current.refresh = true;
    dispatch({ type: 'refreshStarted' });
    Promise.resolve(optionsRef.current.onRefresh?.()).finally(() => {
      busyRef.current.refresh = false;
      dispatch({ type: 'refreshEnded' });
    });
  }, []);

  const handleEndReached = useCallback(() => {
    if (busyRef.current.end) return;
    busyRef.current.end = true;
    dispatch({ type: 'endReachStarted' });
    Promise.resolve(optionsRef.current.onEndReached?.()).finally(() => {
      busyRef.current.end = false;
      dispatch({ type: 'endReachEnded' });
    });
  }, []);

  const handleStartReached = useCallback(() => {
    if (busyRef.current.start) return;
    busyRef.current.start = true;
    dispatch({ type: 'startReachStarted' });
    Promise.resolve(optionsRef.current.onStartReached?.()).finally(() => {
      busyRef.current.start = false;
      dispatch({ type: 'startReachEnded' });
    });
  }, []);

  const handleScroll = useCallback((event: ScrollEventT) => {
    dispatch({ type: 'scrollStarted' });
    optionsRef.current.onScroll?.(event);
    if (scrollIdleTimer.current) timers.clearTimeout(scrollIdleTimer.current);
    scrollIdleTimer.current = timers.setTimeout(() => {
      dispatch({ type: 'scrollEnded' });
    }, optionsRef.current.scrollIdleMs ?? 150);
  }, []);

  const handleViewableItemsChanged = useCallback((info: ViewableInfoT) => {
    optionsRef.current.onViewableItemsChanged?.(info);
  }, []);

  const setData = useCallback(
    (update: ItemT[] | ((prev: ItemT[]) => ItemT[])) =>
      dispatch({ type: 'itemsSet', update }),
    []
  );

  const prepend = useCallback(
    (items: readonly ItemT[]) => dispatch({ type: 'itemsPrepended', items }),
    []
  );

  const append = useCallback(
    (items: readonly ItemT[]) => dispatch({ type: 'itemsAppended', items }),
    []
  );

  const removeItems = useCallback(
    (ids: readonly string[] | ((item: ItemT, index: number) => boolean)) => {
      const match =
        typeof ids === 'function'
          ? ids
          : ((): ((item: ItemT) => boolean) => {
              const set = new Set(ids);
              return (item: ItemT) => set.has(item.id);
            })();
      dispatch({ type: 'itemsRemoved', match });
    },
    []
  );

  const markers = useMemo<ListMarkers>(
    () => ({
      refreshStarted: () => dispatch({ type: 'refreshStarted' }),
      refreshEnded: () => dispatch({ type: 'refreshEnded' }),
      endReachStarted: () => dispatch({ type: 'endReachStarted' }),
      endReachEnded: () => dispatch({ type: 'endReachEnded' }),
      startReachStarted: () => dispatch({ type: 'startReachStarted' }),
      startReachEnded: () => dispatch({ type: 'startReachEnded' }),
      scrollStarted: () => dispatch({ type: 'scrollStarted' }),
      scrollEnded: () => dispatch({ type: 'scrollEnded' }),
    }),
    []
  );

  // Clear any pending scroll-idle timer on unmount.
  useEffect(
    () => () => {
      if (scrollIdleTimer.current) timers.clearTimeout(scrollIdleTimer.current);
    },
    []
  );

  return {
    data: state.data,
    refreshing: state.refreshing,
    loadingMore: state.loadingMore,
    loadingOlder: state.loadingOlder,
    scrolling: state.scrolling,
    handleRefresh,
    handleEndReached,
    handleStartReached,
    handleScroll,
    handleViewableItemsChanged,
    setData,
    prepend,
    append,
    removeItems,
    markers,
  };
}
