import { useCallback, useMemo, useRef, type ComponentRef } from 'react';
import { Platform } from 'react-native';
import ShadowListView, { Commands } from '../ShadowListViewNativeComponent';
import type { RowSelection } from './CellRenderer';
import {
  PAGING_SELECTION_KEYS,
  SELECTION_KEYS,
  needsScrollIntoView,
  nextSelectableIndex,
  pagedSelectableIndex,
} from './keySelection';
import type { RowSource } from './rowSource';
import type { MountedRange } from './useMountedRange';

interface ShadowListViewRef {
  current: ComponentRef<typeof ShadowListView> | null;
}

interface UseKeySelectionOptions<ItemT> {
  enabled: boolean;
  source: RowSource<ItemT>;
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
 * Keyboard selection on macOS and in a terminal. A click gives the list the keyboard. Up and Down
 * select the row above or below and scroll it into view, skipping rows that can't be selected.
 * Return or Space activates the selected row. A terminal also pages with PageUp, PageDown, Home
 * and End.
 */
export function useKeySelection<ItemT>({
  enabled,
  source: rowSource,
  selectionStateRef,
  selection,
  canSelectIndex,
  onActivateItem,
  visibleRangeRef,
  seedAroundIndex,
  viewRef,
}: UseKeySelectionOptions<ItemT>): KeySelectionProps | null {
  const sourceRef = useRef({ rows: rowSource, onActivateItem });
  sourceRef.current = { rows: rowSource, onActivateItem };

  const handleKeyDown = useCallback(
    (event: { nativeEvent: { key: string } }) => {
      const source = sourceRef.current;
      const selectedKey = selectionStateRef.current.keys[0];
      const current =
        selectedKey === undefined ? -1 : source.rows.indexOfKey(selectedKey);
      const { key } = event.nativeEvent;
      const moveTo = (next: number, viewPosition: number) => {
        const nextKey = source.rows.keyAt(next);
        if (nextKey === undefined) return;
        selection.selectOnly(nextKey);
        const view = viewRef.current;
        if (view && needsScrollIntoView(next, visibleRangeRef.current)) {
          seedAroundIndex(next, viewPosition);
          Commands.scrollToRow(view, next, viewPosition, 0, false);
        }
      };
      if (key === 'ArrowUp' || key === 'ArrowDown') {
        const step = key === 'ArrowUp' ? -1 : 1;
        const next = nextSelectableIndex(
          current,
          step,
          source.rows.count,
          canSelectIndex
        );
        moveTo(next, step > 0 ? 1 : 0);
        return;
      }
      if (key !== 'Enter' && key !== ' ') {
        const visible = visibleRangeRef.current;
        const page = visible ? visible.high - visible.low + 1 : 1;
        moveTo(
          pagedSelectableIndex(
            key,
            current,
            page,
            source.rows.count,
            canSelectIndex
          ),
          key === 'PageDown' || key === 'End' ? 1 : 0
        );
        return;
      }
      const item = source.rows.itemAt(current);
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
            keyDownEvents:
              Platform.OS === 'macos' ? SELECTION_KEYS : PAGING_SELECTION_KEYS,
            onKeyDown: handleKeyDown,
            onTouchStart: handleTouchStart,
          }
        : null,
    [enabled, handleKeyDown, handleTouchStart]
  );
}
