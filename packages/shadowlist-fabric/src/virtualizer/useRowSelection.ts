import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { RowSelection } from './CellRenderer';
import { deselectKey, retainKeys, selectKey, selectOnlyKey } from './selection';

/*
 * The empty selection, one array shared by every list.
 */
const NO_KEYS: ReadonlyArray<string> = [];

interface SelectionState {
  keys: ReadonlyArray<string>;
  controlled: boolean;
  multiple: boolean;
  onSelectionChange: ((selectedKeys: string[]) => void) | undefined;
  canSelect: (key: string) => boolean;
}

interface UseRowSelectionOptions {
  keyToIndex: ReadonlyMap<string, number>;
  selectedKeys: ReadonlyArray<string> | undefined;
  allowsMultipleSelection: boolean;
  onSelectionChange: ((selectedKeys: string[]) => void) | undefined;
  canSelect: (key: string) => boolean;
}

interface UseRowSelectionResult {
  selectedKeySet: ReadonlySet<string>;
  selection: RowSelection;
  selectionStateRef: { current: SelectionState };
}

/*
 * Selection by key, controlled through selectedKeys or kept here. Rows read whether they are
 * selected and select or deselect themselves. A row canSelect turns down stays as it is.
 */
export function useRowSelection({
  keyToIndex,
  selectedKeys,
  allowsMultipleSelection,
  onSelectionChange,
  canSelect,
}: UseRowSelectionOptions): UseRowSelectionResult {
  const [ownSelectedKeys, setOwnSelectedKeys] =
    useState<ReadonlyArray<string>>(NO_KEYS);
  const currentSelectedKeys = selectedKeys ?? ownSelectedKeys;
  const selectedKeySet = useMemo(
    () => new Set(currentSelectedKeys),
    [currentSelectedKeys]
  );
  const selectionStateRef = useRef<SelectionState>({
    keys: currentSelectedKeys,
    controlled: selectedKeys !== undefined,
    multiple: allowsMultipleSelection,
    onSelectionChange,
    canSelect,
  });
  selectionStateRef.current = {
    keys: currentSelectedKeys,
    controlled: selectedKeys !== undefined,
    multiple: allowsMultipleSelection,
    onSelectionChange,
    canSelect,
  };
  const applySelection = useCallback((next: ReadonlyArray<string>) => {
    const state = selectionStateRef.current;
    if (next === state.keys) return;
    state.keys = next;
    if (!state.controlled) setOwnSelectedKeys(next);
    state.onSelectionChange?.([...next]);
  }, []);
  const selection = useMemo<RowSelection>(
    () => ({
      select: (key: string) => {
        const state = selectionStateRef.current;
        if (!state.canSelect(key)) return;
        applySelection(selectKey(state.keys, key, state.multiple));
      },
      deselect: (key: string) => {
        applySelection(deselectKey(selectionStateRef.current.keys, key));
      },
      selectOnly: (key: string) => {
        const state = selectionStateRef.current;
        if (!state.canSelect(key)) return;
        applySelection(selectOnlyKey(state.keys, key));
      },
    }),
    [applySelection]
  );
  // A removed row leaves the selection.
  useEffect(() => {
    applySelection(retainKeys(selectionStateRef.current.keys, keyToIndex));
  }, [keyToIndex, applySelection]);

  return { selectedKeySet, selection, selectionStateRef };
}
