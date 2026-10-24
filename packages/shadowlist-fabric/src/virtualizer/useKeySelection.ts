import { useCallback, useMemo, useRef, type ComponentRef } from 'react';
import ShadowListView, { Commands } from '../ShadowListViewNativeComponent';
import type { RowSelection } from './CellRenderer';
import {
  SELECTION_KEYS,
  needsScrollIntoView,
  nextSelectableIndex,
} from './keySelection';
import type { MountedRange } from './useMountedRange';

interface ShadowListViewRef {
  current: ComponentRef<typeof ShadowListView> | null;
}

interface UseKeySelectionOptions<ItemT> {
  enabled: boolean;
  data: ReadonlyArray<ItemT>;
  keys: ReadonlyArray<string>;
  keyToIndex: ReadonlyMap<string, number>;
  selectionStateRef: { current: { keys: ReadonlyArray<string> } };
  selection: RowSelection;
  canSelectIndex: (index: number) => boolean;
  onActivateItem: ((info: { item: ItemT; index: number }) => void) | undefined;
  visibleRangeRef: { current: MountedRange | null };
  seedAroundIndex: (index: number, viewPosition: number) => void;
  viewRef: ShadowListViewRef;
}

/*
 * The props that give the native list the keyboard.
 */
export interface KeySelectionProps {
  focusable: boolean;
  enableFocusRing: boolean;
  keyDownEvents: typeof SELECTION_KEYS;
  onKeyDown: (event: { nativeEvent: { key: string } }) => void;
  onTouchStart: () => void;
}

/*
 * Keyboard selection on macOS. A click gives the list the keyboard. Up and Down select the row
 * above or below and scroll it into view, skipping rows that can't be selected. Return or
 * Space activates the selected row.
 */
export function useKeySelection<ItemT>({
  enabled,
  data,
  keys,
  keyToIndex,
  selectionStateRef,
  selection,
  canSelectIndex,
  onActivateItem,
  visibleRangeRef,
  seedAroundIndex,
  viewRef,
}: UseKeySelectionOptions<ItemT>): KeySelectionProps | null {
  const sourceRef = useRef({ data, keys, keyToIndex, onActivateItem });
  sourceRef.current = { data, keys, keyToIndex, onActivateItem };

  const handleKeyDown = useCallback(
    (event: { nativeEvent: { key: string } }) => {
      const source = sourceRef.current;
      const selectedKey = selectionStateRef.current.keys[0];
      const current =
        selectedKey === undefined
          ? -1
          : (source.keyToIndex.get(selectedKey) ?? -1);
      const { key } = event.nativeEvent;
      if (key === 'ArrowUp' || key === 'ArrowDown') {
        const step = key === 'ArrowUp' ? -1 : 1;
        const next = nextSelectableIndex(
          current,
          step,
          source.data.length,
          canSelectIndex
        );
        const nextKey = source.keys[next];
        if (nextKey === undefined) return;
        selection.selectOnly(nextKey);
        const view = viewRef.current;
        if (view && needsScrollIntoView(next, visibleRangeRef.current)) {
          const viewPosition = step > 0 ? 1 : 0;
          seedAroundIndex(next, viewPosition);
          Commands.scrollToRow(view, next, viewPosition, 0, false);
        }
        return;
      }
      const item = source.data[current];
      if (item !== undefined) {
        source.onActivateItem?.({ item, index: current });
      }
    },
    [
      selectionStateRef,
      selection,
      canSelectIndex,
      visibleRangeRef,
      seedAroundIndex,
      viewRef,
    ]
  );

  // focus() sends the focus command, which the native list hands to its base view.
  const handleTouchStart = useCallback(() => {
    (viewRef.current as { focus?: () => void } | null)?.focus?.();
  }, [viewRef]);

  return useMemo(
    () =>
      enabled
        ? {
            focusable: true,
            enableFocusRing: false,
            keyDownEvents: SELECTION_KEYS,
            onKeyDown: handleKeyDown,
            onTouchStart: handleTouchStart,
          }
        : null,
    [enabled, handleKeyDown, handleTouchStart]
  );
}
