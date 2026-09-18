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
  ElementT,
  ScrollEventT = unknown,
  ViewableInfoT = unknown,
> {
  initialData?: readonly ElementT[];
  onRefresh?: () => void | Promise<void>;
  onEndReached?: () => void | Promise<void>;
  onStartReached?: () => void | Promise<void>;
  onScroll?: (event: ScrollEventT) => void;
  onViewableItemsChanged?: (info: ViewableInfoT) => void;
  /*
   * Receives a throw or rejection from onRefresh / onEndReached / onStartReached. Without
   * it the rejection is left unhandled; the loading flag resets either way.
   */
  onError?: (error: unknown) => void;
  scrollIdleMs?: number;
}

interface ListState<ElementT> {
  data: ElementT[];
  refreshing: boolean;
  loadingMore: boolean;
  loadingOlder: boolean;
  scrolling: boolean;
}

/* The reducer actions, named as the start/end markers the state moves between. */
type ListAction<ElementT> =
  | { type: 'refreshStarted' }
  | { type: 'refreshEnded' }
  | { type: 'endReachStarted' }
  | { type: 'endReachEnded' }
  | { type: 'startReachStarted' }
  | { type: 'startReachEnded' }
  | { type: 'scrollStarted' }
  | { type: 'scrollEnded' }
  | {
      type: 'itemsSet';
      update: ElementT[] | ((prev: ElementT[]) => ElementT[]);
    }
  | { type: 'itemsPrepended'; items: readonly ElementT[] }
  | { type: 'itemsAppended'; items: readonly ElementT[] }
  | { type: 'itemsUpserted'; items: readonly ElementT[] }
  | {
      type: 'itemsRemoved';
      match: (element: ElementT, index: number) => boolean;
    };

/* Flag actions return the same state object when nothing changes, so an already-true
 * `scrollStarted` on every scroll event doesn't trigger a re-render. */
function listReducer<ElementT extends { id: string }>(
  state: ListState<ElementT>,
  action: ListAction<ElementT>
): ListState<ElementT> {
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
    case 'itemsSet': {
      const data =
        typeof action.update === 'function'
          ? action.update(state.data)
          : action.update;
      // An updater that changed nothing returns `prev`; skip the re-render.
      return data === state.data ? state : { ...state, data };
    }
    case 'itemsPrepended':
      return { ...state, data: [...action.items, ...state.data] };
    case 'itemsAppended':
      return { ...state, data: [...state.data, ...action.items] };
    case 'itemsUpserted': {
      if (action.items.length === 0) return state;
      const pending = new Map(action.items.map((item) => [item.id, item]));
      const data = state.data.map((element) => {
        const replacement = pending.get(element.id);
        if (replacement === undefined) return element;
        pending.delete(element.id);
        return replacement;
      });
      return { ...state, data: [...data, ...pending.values()] };
    }
    case 'itemsRemoved':
      return {
        ...state,
        data: state.data.filter(
          (element, index) => !action.match(element, index)
        ),
      };
    default:
      return state;
  }
}

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
  ElementT,
  ScrollEventT = unknown,
  ViewableInfoT = unknown,
> {
  data: ElementT[];
  refreshing: boolean;
  loadingMore: boolean;
  loadingOlder: boolean;
  scrolling: boolean;

  handleRefresh: () => void;
  handleEndReached: () => void;
  handleStartReached: () => void;
  handleScroll: (event: ScrollEventT) => void;
  handleViewableItemsChanged: (info: ViewableInfoT) => void;

  setData: (update: ElementT[] | ((prev: ElementT[]) => ElementT[])) => void;
  prepend: (items: readonly ElementT[]) => void;
  append: (items: readonly ElementT[]) => void;
  // Replaces the rows whose id exists in place and appends the rest.
  upsertItems: (items: readonly ElementT[]) => void;
  // Replaces one row by id; a missing id is a no-op.
  updateItem: (id: string, update: (element: ElementT) => ElementT) => void;
  removeItems: (
    ids: readonly string[] | ((element: ElementT, index: number) => boolean)
  ) => void;

  markers: ListMarkers;
}

export function useListController<
  ElementT extends { id: string },
  ScrollEventT = unknown,
  ViewableInfoT = unknown,
>(
  options: UseListControllerOptions<ElementT, ScrollEventT, ViewableInfoT> = {}
): ListController<ElementT, ScrollEventT, ViewableInfoT> {
  const [state, dispatch] = useReducer(
    listReducer<ElementT>,
    options.initialData,
    (seed): ListState<ElementT> => ({
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

  /*
   * Synchronous busy guards. dispatch only flips a flag on the next render, so a
   * second call in the same tick would slip past a state-based check.
   */
  const busyRef = useRef({ refresh: false, end: false, start: false });

  const scrollIdleTimer = useRef<number | null>(null);

  /*
   * Runs one consumer callback and then `settle`. The callback is invoked inside the `.then`,
   * not eagerly, so a *synchronous* throw still becomes a rejection this chain observes --
   * otherwise `settle` would never run and the busy flag / loading UI would stay stuck true.
   */
  const run = useCallback(
    (callback: () => void | Promise<void>, settle: () => void) => {
      const pending = Promise.resolve().then(callback).finally(settle);
      const { onError } = optionsRef.current;
      if (onError) pending.catch(onError);
    },
    []
  );

  const handleRefresh = useCallback(() => {
    if (busyRef.current.refresh) return;
    busyRef.current.refresh = true;
    dispatch({ type: 'refreshStarted' });
    run(
      () => optionsRef.current.onRefresh?.(),
      () => {
        busyRef.current.refresh = false;
        dispatch({ type: 'refreshEnded' });
      }
    );
  }, [run]);

  const handleEndReached = useCallback(() => {
    if (busyRef.current.end) return;
    busyRef.current.end = true;
    dispatch({ type: 'endReachStarted' });
    run(
      () => optionsRef.current.onEndReached?.(),
      () => {
        busyRef.current.end = false;
        dispatch({ type: 'endReachEnded' });
      }
    );
  }, [run]);

  const handleStartReached = useCallback(() => {
    if (busyRef.current.start) return;
    busyRef.current.start = true;
    dispatch({ type: 'startReachStarted' });
    run(
      () => optionsRef.current.onStartReached?.(),
      () => {
        busyRef.current.start = false;
        dispatch({ type: 'startReachEnded' });
      }
    );
  }, [run]);

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
    (update: ElementT[] | ((prev: ElementT[]) => ElementT[])) =>
      dispatch({ type: 'itemsSet', update }),
    []
  );

  const prepend = useCallback(
    (items: readonly ElementT[]) => dispatch({ type: 'itemsPrepended', items }),
    []
  );

  const append = useCallback(
    (items: readonly ElementT[]) => dispatch({ type: 'itemsAppended', items }),
    []
  );

  const upsertItems = useCallback(
    (items: readonly ElementT[]) => dispatch({ type: 'itemsUpserted', items }),
    []
  );

  const updateItem = useCallback(
    (id: string, update: (element: ElementT) => ElementT) =>
      dispatch({
        type: 'itemsSet',
        update: (prev) => {
          const index = prev.findIndex((element) => element.id === id);
          if (index === -1) return prev;
          const next = [...prev];
          next[index] = update(prev[index]!);
          return next;
        },
      }),
    []
  );

  const removeItems = useCallback(
    (
      ids: readonly string[] | ((element: ElementT, index: number) => boolean)
    ) => {
      const match =
        typeof ids === 'function'
          ? ids
          : ((): ((element: ElementT) => boolean) => {
              const set = new Set(ids);
              return (element: ElementT) => set.has(element.id);
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
    upsertItems,
    updateItem,
    removeItems,
    markers,
  };
}
