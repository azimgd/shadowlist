import {
  type ComponentRef,
  useCallback,
  useEffect,
  useLayoutEffect,
  useReducer,
  useRef,
} from 'react';
import { Platform } from 'react-native';
import ShadowListView, { Commands } from '../ShadowListViewNativeComponent';
import { slTrace, slTraceEnabled } from './helpers';

/*
 * What gets held: the list's rows, of which only the count is read here.
 */
interface Rows {
  readonly count: number;
}

interface UseRefreshDeferOptions<RowsT extends Rows> {
  data: RowsT;
  refreshing: boolean;
  onRefresh: (() => void) | undefined;
  inverted: boolean;
  horizontal: boolean;
  viewRef: { current: ComponentRef<typeof ShadowListView> | null };
}

interface UseRefreshDeferResult<RowsT extends Rows> {
  data: RowsT;
  handleRefreshSettle: () => void;
}

type RefreshState<RowsT> =
  | { phase: 'idle' }
  | { phase: 'refreshing' | 'ended' | 'awaiting'; shown: RowsT | null }
  | {
      phase: 'refreshingHeld' | 'endedHeld' | 'awaitingHeld';
      shown: RowsT;
      held: RowsT;
    };

type RefreshEvent<RowsT> =
  | {
      type: 'render';
      enabled: boolean;
      refreshing: boolean;
      shown: RowsT;
      data: RowsT;
    }
  | { type: 'request' }
  | { type: 'settle' };

/*
 * A held phase records that data actually changed, even if it later returns to the shown
 * rows. Its last held value deduplicates the trace; release always uses the current prop.
 */
function transitionRefresh<RowsT>(
  state: RefreshState<RowsT>,
  event: RefreshEvent<RowsT>
): RefreshState<RowsT> {
  const refreshing =
    state.phase === 'refreshing' || state.phase === 'refreshingHeld';
  if (event.type === 'settle') {
    return refreshing
      ? { phase: 'refreshing', shown: null }
      : { phase: 'idle' };
  }
  if (event.type === 'request') {
    if (state.phase === 'endedHeld') return { ...state, phase: 'awaitingHeld' };
    return state.phase === 'ended' ? { ...state, phase: 'awaiting' } : state;
  }
  if (!event.enabled) {
    return event.refreshing
      ? { phase: 'refreshing', shown: null }
      : { phase: 'idle' };
  }
  if (event.refreshing && !refreshing) {
    state =
      'held' in state
        ? { ...state, phase: 'refreshingHeld' }
        : {
            phase: 'refreshing',
            shown:
              state.phase === 'idle'
                ? event.shown
                : (state.shown ?? event.shown),
          };
  } else if (!event.refreshing && refreshing) {
    state =
      'held' in state
        ? { ...state, phase: 'endedHeld' }
        : {
            phase: 'ended',
            shown: state.phase === 'idle' ? null : state.shown,
          };
  }
  if (
    state.phase !== 'idle' &&
    state.shown !== null &&
    event.data !== state.shown
  ) {
    const phase =
      state.phase === 'refreshing' || state.phase === 'refreshingHeld'
        ? 'refreshingHeld'
        : state.phase === 'ended' || state.phase === 'endedHeld'
          ? 'endedHeld'
          : 'awaitingHeld';
    return { phase, shown: state.shown, held: event.data };
  }
  return state;
}

/*
 * Hold changes during refresh until native reports that the spinner and list are at rest.
 * Only vertical, non inverted lists with onRefresh defer their data.
 */
export function useRefreshDefer<RowsT extends Rows>({
  data: dataProp,
  refreshing,
  onRefresh,
  inverted,
  horizontal,
  viewRef,
}: UseRefreshDeferOptions<RowsT>): UseRefreshDeferResult<RowsT> {
  const refreshDeferEnabled =
    Platform.OS !== 'macos' && !!onRefresh && !inverted && !horizontal;
  const refreshRef = useRef<RefreshState<RowsT>>(
    refreshing ? { phase: 'refreshing', shown: null } : { phase: 'idle' }
  );
  const shownDataRef = useRef(dataProp);
  const [, forceRender] = useReducer((count: number) => count + 1, 0);
  const previous = refreshRef.current;
  refreshRef.current = transitionRefresh(previous, {
    type: 'render',
    enabled: refreshDeferEnabled,
    refreshing,
    shown: shownDataRef.current,
    data: dataProp,
  });
  const state = refreshRef.current;
  if (
    'held' in state &&
    (!('held' in previous) || previous.held !== state.held) &&
    slTraceEnabled()
  ) {
    slTrace(`refresh hold n=${state.held.count}`);
  }
  const data = state.phase === 'idle' ? dataProp : (state.shown ?? dataProp);

  useLayoutEffect(() => {
    shownDataRef.current = data;
  });

  const handleRefreshSettle = useCallback(() => {
    const current = refreshRef.current;
    const held = 'held' in current;
    refreshRef.current = transitionRefresh(current, { type: 'settle' });
    if (slTraceEnabled()) {
      slTrace(`refresh settle held=${held ? 1 : 0}`);
    }
    if (held) forceRender();
  }, []);

  /*
   * Request even when a refresh was too short to mount its native prop. Without a view,
   * nothing can move and the data can be released immediately.
   */
  useEffect(() => {
    if (
      refreshRef.current.phase !== 'ended' &&
      refreshRef.current.phase !== 'endedHeld'
    )
      return;
    refreshRef.current = transitionRefresh(refreshRef.current, {
      type: 'request',
    });
    const view = viewRef.current;
    if (view) {
      Commands.awaitRefreshSettle(view);
    } else {
      handleRefreshSettle();
    }
  });

  return { data, handleRefreshSettle };
}
