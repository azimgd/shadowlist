import { useCallback, useEffect, useRef, type ComponentRef } from 'react';
import ShadowListView, {
  Commands,
  type OnAnchorState,
} from '../ShadowListViewNativeComponent';
import type { AnchorState } from '../types';
import { keysIdentityOf, type RowSource } from './rowSource';

/*
 * Longest wait for native to answer getAnchorState.
 */
const ANCHOR_STATE_TIMEOUT_MS = 1000;

interface UseAnchorStateOptions {
  viewRef: { current: ComponentRef<typeof ShadowListView> | null };
  source: RowSource<unknown>;
  seedAroundIndex: (index: number, viewPosition: number) => void;
}

interface UseAnchorStateResult {
  handleAnchorState: (event: { nativeEvent: OnAnchorState }) => void;
  requestAnchorState: () => Promise<AnchorState | null>;
  restoreAnchorState: (state: AnchorState) => void;
}

/*
 * getAnchorState asks native and waits for onAnchorState. restoreAnchorState scrolls the
 * anchor's row back, now or once a data change brings its key. The commands are new on every
 * render and read through the command source.
 */
export function useAnchorState({
  viewRef,
  source,
  seedAroundIndex,
}: UseAnchorStateOptions): UseAnchorStateResult {
  const anchorWaitersRef = useRef<((state: AnchorState | null) => void)[]>([]);
  const handleAnchorState = useCallback(
    (event: { nativeEvent: OnAnchorState }) => {
      const { found, key, offset } = event.nativeEvent;
      const waiters = anchorWaitersRef.current;
      anchorWaitersRef.current = [];
      waiters.forEach((resolve) => resolve(found ? { key, offset } : null));
    },
    []
  );
  const pendingAnchorRef = useRef<AnchorState | null>(null);

  const requestAnchorState = () =>
    new Promise<AnchorState | null>((resolve) => {
      const view = viewRef.current;
      if (!view) {
        resolve(null);
        return;
      }
      let settled = false;
      const finish = (state: AnchorState | null) => {
        if (settled) return;
        settled = true;
        resolve(state);
      };
      anchorWaitersRef.current.push(finish);
      setTimeout(() => finish(null), ANCHOR_STATE_TIMEOUT_MS);
      Commands.requestAnchorState(view);
    });

  const restoreAnchorState = (state: AnchorState) => {
    const index = source.indexOfKey(state.key);
    const view = viewRef.current;
    if (index < 0 || !view) {
      pendingAnchorRef.current = state;
      return;
    }
    pendingAnchorRef.current = null;
    seedAroundIndex(index, 0);
    Commands.scrollToRow(view, index, 0, -state.offset, false);
  };

  // The pending anchor is restored by the latest render's restoreAnchorState.
  const restoreAnchorStateRef = useRef(restoreAnchorState);
  restoreAnchorStateRef.current = restoreAnchorState;
  const sourceRef = useRef(source);
  sourceRef.current = source;
  const keysIdentity = keysIdentityOf(source);
  useEffect(() => {
    const pending = pendingAnchorRef.current;
    if (pending && sourceRef.current.indexOfKey(pending.key) >= 0) {
      restoreAnchorStateRef.current(pending);
    }
  }, [keysIdentity]);

  return { handleAnchorState, requestAnchorState, restoreAnchorState };
}
