import { useCallback, useEffect, useRef, type ComponentRef } from 'react';
import ShadowListView, {
  Commands,
  type OnAnchorState,
} from '../ShadowListViewNativeComponent';
import type { AnchorState } from '../types';
import type { RowIndexStore } from './ElementRenderer';

/*
 * Longest wait for native to answer getAnchorState.
 */
const ANCHOR_STATE_TIMEOUT_MS = 1000;

interface UseAnchorStateOptions {
  viewRef: { current: ComponentRef<typeof ShadowListView> | null };
  rowIndex: RowIndexStore;
  keyToIndex: ReadonlyMap<string, number>;
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
  rowIndex,
  keyToIndex,
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
    const index = rowIndex.keyToIndex.get(state.key);
    const view = viewRef.current;
    if (index === undefined || !view) {
      pendingAnchorRef.current = state;
      return;
    }
    pendingAnchorRef.current = null;
    seedAroundIndex(index, 0);
    Commands.scrollToItem(view, index, 0, -state.offset, false);
  };

  // The pending anchor is restored by the latest render's restoreAnchorState.
  const restoreAnchorStateRef = useRef(restoreAnchorState);
  restoreAnchorStateRef.current = restoreAnchorState;
  useEffect(() => {
    const pending = pendingAnchorRef.current;
    if (pending && keyToIndex.has(pending.key)) {
      restoreAnchorStateRef.current(pending);
    }
  }, [keyToIndex]);

  return { handleAnchorState, requestAnchorState, restoreAnchorState };
}
