import { useReducer, useRef, useCallback, useMemo, useEffect } from 'react';

/* The tsconfig has no DOM or Node lib, so timer globals have no types. Read them off
 * globalThis without destructuring, so the calls stay bound to the global. */
const timers = globalThis as unknown as {
  setTimeout: (handler: () => void, timeout: number) => number;
  clearTimeout: (handle: number) => void;
};

/*
 * Holds the list state every screen ends up writing again. The refreshing, loading more
 * and loading older flags, the guards around them, and the data with prepend, append and
 * remove. Pass the handle callbacks straight to the list. Each one sets its flag while your
 * async work runs and never fires twice.
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
   * Gets any error from onRefresh, onEndReached or onStartReached. Without it the rejection
   * goes unhandled. The loading flag resets either way.
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
      update: ElementT[] | ((previous: ElementT[]) => ElementT[]);
    }
  | { type: 'itemsPrepended'; items: readonly ElementT[] }
  | { type: 'itemsAppended'; items: readonly ElementT[] }
  | { type: 'itemsUpserted'; items: readonly ElementT[] }
  | {
      type: 'itemsRemoved';
      match: (element: ElementT, index: number) => boolean;
    };

/* Flag actions return the same state when nothing changes, so scrollStarted on every
 * scroll event doesn't re-render. */
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
      // The updater changed nothing, so skip the re-render.
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

  setData: (
    update: ElementT[] | ((previous: ElementT[]) => ElementT[])
  ) => void;
  prepend: (items: readonly ElementT[]) => void;
  append: (items: readonly ElementT[]) => void;
  // Replaces rows whose id already exists and appends the rest.
  upsertItems: (items: readonly ElementT[]) => void;
  // Replaces one row by id. Does nothing if the id is missing.
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

  // Keep the latest options in a ref so the handlers stay stable.
  const optionsRef = useRef(options);
  optionsRef.current = options;

  /*
   * Busy guards that flip right away. State only changes on the next render, so a second
   * call in the same tick would slip past it.
   */
  const busyRef = useRef({ refresh: false, end: false, start: false });

  const scrollIdleTimer = useRef<number | null>(null);
  /*
   * Whether scrollStarted went out without a scrollEnded after it, and when the last scroll
   * event came. A scroll event then costs no dispatch and no timer call while scrolling
   * goes on. Dispatching per event scheduled a render each time, and clearing and setting
   * the idle timer per event is two native calls.
   */
  const scrollingRef = useRef(false);
  const lastScrollAtRef = useRef(0);

  /*
   * Runs the callback, then settle. The callback runs inside then, so a synchronous throw
   * still becomes a rejection here. Otherwise settle never runs and the loading flag stays stuck.
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

  /*
   * Ends scrolling once no event came for scrollIdleMs. One timer per scroll: when it fires
   * early because events kept coming, it waits out the rest.
   */
  const armScrollIdle = useCallback((delay: number) => {
    scrollIdleTimer.current = timers.setTimeout(() => {
      scrollIdleTimer.current = null;
      const idleMs = optionsRef.current.scrollIdleMs ?? 150;
      const remaining = lastScrollAtRef.current + idleMs - Date.now();
      if (remaining > 0) {
        armScrollIdle(remaining);
        return;
      }
      scrollingRef.current = false;
      dispatch({ type: 'scrollEnded' });
    }, delay);
  }, []);

  const handleScroll = useCallback(
    (event: ScrollEventT) => {
      lastScrollAtRef.current = Date.now();
      if (!scrollingRef.current) {
        scrollingRef.current = true;
        dispatch({ type: 'scrollStarted' });
      }
      optionsRef.current.onScroll?.(event);
      if (scrollIdleTimer.current === null) {
        armScrollIdle(optionsRef.current.scrollIdleMs ?? 150);
      }
    },
    [armScrollIdle]
  );

  const handleViewableItemsChanged = useCallback((info: ViewableInfoT) => {
    optionsRef.current.onViewableItemsChanged?.(info);
  }, []);

  const setData = useCallback(
    (update: ElementT[] | ((previous: ElementT[]) => ElementT[])) =>
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
        update: (previous) => {
          const index = previous.findIndex((element) => element.id === id);
          if (index === -1) return previous;
          const next = [...previous];
          next[index] = update(previous[index]!);
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
      scrollStarted: () => {
        scrollingRef.current = true;
        dispatch({ type: 'scrollStarted' });
      },
      scrollEnded: () => {
        scrollingRef.current = false;
        dispatch({ type: 'scrollEnded' });
      },
    }),
    []
  );

  useEffect(
    () => () => {
      if (scrollIdleTimer.current !== null) {
        timers.clearTimeout(scrollIdleTimer.current);
        scrollIdleTimer.current = null;
      }
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
