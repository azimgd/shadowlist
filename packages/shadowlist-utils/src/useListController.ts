import { useReducer, useRef, useCallback, useMemo, useEffect } from 'react';

/* The tsconfig has no DOM or Node lib and timer globals have no types. Read them off
 * globalThis without destructuring to keep the calls bound to the global. */
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
  ItemT,
  ScrollEventT = unknown,
  ViewableInfoT = unknown,
> {
  initialData?: readonly ItemT[];
  keyExtractor?: (item: ItemT) => string;
  onRefresh?: () => void | Promise<void>;
  onEndReached?: () => void | Promise<void>;
  onStartReached?: () => void | Promise<void>;
  onScroll?: (event: ScrollEventT) => void;
  onViewableItemsChanged?: (info: ViewableInfoT) => void;
  onError?: (error: unknown) => void;
  scrollIdleMs?: number;
}

interface ListState<ItemT> {
  data: ItemT[];
  refreshing: boolean;
  loadingMore: boolean;
  loadingOlder: boolean;
  scrolling: boolean;
}

type ListAction<ItemT> =
  | { type: 'refreshStarted' }
  | { type: 'refreshEnded' }
  | { type: 'endReachedStarted' }
  | { type: 'endReachedEnded' }
  | { type: 'startReachedStarted' }
  | { type: 'startReachedEnded' }
  | { type: 'scrollStarted' }
  | { type: 'scrollEnded' }
  | {
      type: 'itemsSet';
      update: ItemT[] | ((previous: ItemT[]) => ItemT[]);
    }
  | { type: 'itemsPrepended'; items: readonly ItemT[] }
  | { type: 'itemsAppended'; items: readonly ItemT[] }
  | {
      type: 'itemsUpserted';
      items: readonly ItemT[];
      keyExtractor: (item: ItemT) => string;
    }
  | {
      type: 'itemsRemoved';
      match: (item: ItemT, index: number) => boolean;
    };

/* Flag actions return the same state when nothing changes. scrollStarted on every
 * scroll event doesn't re-render. */
function listReducer<ItemT>(
  state: ListState<ItemT>,
  action: ListAction<ItemT>
): ListState<ItemT> {
  switch (action.type) {
    case 'refreshStarted':
      return state.refreshing ? state : { ...state, refreshing: true };
    case 'refreshEnded':
      return state.refreshing ? { ...state, refreshing: false } : state;
    case 'endReachedStarted':
      return state.loadingMore ? state : { ...state, loadingMore: true };
    case 'endReachedEnded':
      return state.loadingMore ? { ...state, loadingMore: false } : state;
    case 'startReachedStarted':
      return state.loadingOlder ? state : { ...state, loadingOlder: true };
    case 'startReachedEnded':
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
      // The updater changed nothing. Skip the re-render.
      return data === state.data ? state : { ...state, data };
    }
    case 'itemsPrepended':
      return { ...state, data: [...action.items, ...state.data] };
    case 'itemsAppended':
      return { ...state, data: [...state.data, ...action.items] };
    case 'itemsUpserted': {
      if (action.items.length === 0) return state;
      const { keyExtractor } = action;
      const pending = new Map(
        action.items.map((item) => [keyExtractor(item), item])
      );
      const data = state.data.map((item) => {
        const key = keyExtractor(item);
        const replacement = pending.get(key);
        if (replacement === undefined) return item;
        pending.delete(key);
        return replacement;
      });
      return { ...state, data: [...data, ...pending.values()] };
    }
    case 'itemsRemoved':
      return {
        ...state,
        data: state.data.filter((item, index) => !action.match(item, index)),
      };
    default:
      return state;
  }
}

export interface ListMarkers {
  refreshStarted: () => void;
  refreshEnded: () => void;
  endReachedStarted: () => void;
  endReachedEnded: () => void;
  startReachedStarted: () => void;
  startReachedEnded: () => void;
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

  setItems: (update: ItemT[] | ((previous: ItemT[]) => ItemT[])) => void;
  prependItems: (items: readonly ItemT[]) => void;
  appendItems: (items: readonly ItemT[]) => void;
  upsertItems: (items: readonly ItemT[]) => void;
  updateItem: (key: string, update: (item: ItemT) => ItemT) => void;
  removeItems: (
    keys: readonly string[] | ((item: ItemT, index: number) => boolean)
  ) => void;

  markers: ListMarkers;
}

/*
 * The key of an item without a keyExtractor: its id.
 */
function defaultKeyExtractor(item: unknown): string {
  return String((item as { id?: unknown }).id);
}

export function useListController<
  ItemT,
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

  // Keep the latest options in a ref so the handlers stay stable.
  const optionsRef = useRef(options);
  optionsRef.current = options;

  /*
   * Busy guards that flip right away. State only changes on the next render. A second
   * call in the same tick would slip past it.
   */
  const busyRef = useRef({ refresh: false, end: false, start: false });

  const scrollIdleTimer = useRef<number | null>(null);
  /*
   * Whether scrollStarted went out without a scrollEnded after it, and when the last scroll
   * event came. A scroll event then costs no dispatch and no timer call while scrolling
   * goes on. Dispatching per event would schedule a render each time, and clearing and
   * setting the idle timer per event is two native calls.
   */
  const scrollingRef = useRef(false);
  const lastScrollAtRef = useRef(0);

  /*
   * Runs the callback, then settle. The callback runs inside then. A synchronous throw
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
    dispatch({ type: 'endReachedStarted' });
    run(
      () => optionsRef.current.onEndReached?.(),
      () => {
        busyRef.current.end = false;
        dispatch({ type: 'endReachedEnded' });
      }
    );
  }, [run]);

  const handleStartReached = useCallback(() => {
    if (busyRef.current.start) return;
    busyRef.current.start = true;
    dispatch({ type: 'startReachedStarted' });
    run(
      () => optionsRef.current.onStartReached?.(),
      () => {
        busyRef.current.start = false;
        dispatch({ type: 'startReachedEnded' });
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

  const keyOf = useCallback(
    (item: ItemT) =>
      (optionsRef.current.keyExtractor ?? defaultKeyExtractor)(item),
    []
  );

  const setItems = useCallback(
    (update: ItemT[] | ((previous: ItemT[]) => ItemT[])) =>
      dispatch({ type: 'itemsSet', update }),
    []
  );

  const prependItems = useCallback(
    (items: readonly ItemT[]) => dispatch({ type: 'itemsPrepended', items }),
    []
  );

  const appendItems = useCallback(
    (items: readonly ItemT[]) => dispatch({ type: 'itemsAppended', items }),
    []
  );

  const upsertItems = useCallback(
    (items: readonly ItemT[]) =>
      dispatch({ type: 'itemsUpserted', items, keyExtractor: keyOf }),
    [keyOf]
  );

  const updateItem = useCallback(
    (key: string, update: (item: ItemT) => ItemT) =>
      dispatch({
        type: 'itemsSet',
        update: (previous) => {
          const index = previous.findIndex((item) => keyOf(item) === key);
          if (index === -1) return previous;
          const next = [...previous];
          next[index] = update(previous[index]!);
          return next;
        },
      }),
    [keyOf]
  );

  const removeItems = useCallback(
    (keys: readonly string[] | ((item: ItemT, index: number) => boolean)) => {
      const match =
        typeof keys === 'function'
          ? keys
          : ((): ((item: ItemT) => boolean) => {
              const set = new Set(keys);
              return (item: ItemT) => set.has(keyOf(item));
            })();
      dispatch({ type: 'itemsRemoved', match });
    },
    [keyOf]
  );

  const markers = useMemo<ListMarkers>(
    () => ({
      refreshStarted: () => dispatch({ type: 'refreshStarted' }),
      refreshEnded: () => dispatch({ type: 'refreshEnded' }),
      endReachedStarted: () => dispatch({ type: 'endReachedStarted' }),
      endReachedEnded: () => dispatch({ type: 'endReachedEnded' }),
      startReachedStarted: () => dispatch({ type: 'startReachedStarted' }),
      startReachedEnded: () => dispatch({ type: 'startReachedEnded' }),
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
    setItems,
    prependItems,
    appendItems,
    upsertItems,
    updateItem,
    removeItems,
    markers,
  };
}
