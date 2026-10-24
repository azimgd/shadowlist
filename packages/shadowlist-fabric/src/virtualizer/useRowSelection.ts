import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { RowSelection } from './CellRenderer';
import type { ListStore } from './nativeStore';
import { keysIdentityOf, type RowSource } from './rowSource';

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
  source: RowSource<unknown>;
  store: ListStore | null;
  selectedKeys: ReadonlyArray<string> | undefined;
  allowsMultipleSelection: boolean;
  onSelectionChange: ((selectedKeys: string[]) => void) | undefined;
  canSelect: (key: string) => boolean;
}

interface UseRowSelectionResult {
  selectedKeySet: ReadonlySet<string>;
  selection: RowSelection;
  selectionStateRef: { current: SelectionState };
  selectedIndices: () => number[];
}

/*
 * Selection by key, controlled through selectedKeys or kept here. Rows read whether they are
 * selected and select or deselect themselves. A row canSelect turns down stays as it is.
 *
 * The rules are the core's ListSelection, which the list's native store holds, see
 * ShadowListStoreJsi.h. Each change returns the selected keys React renders, or null when it
 * changed nothing. Before a change the native selection takes the keys rendered now when they
 * differ from the ones it gave, like a controlled selectedKeys the parent changed or kept.
 * Without the store module, as in tests that do not install one, rows show selectedKeys and
 * changes do nothing.
 */
export function useRowSelection({
  source,
  store,
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
  const storeRef = useRef(store);
  storeRef.current = store;
  const sourceRef = useRef(source);
  sourceRef.current = source;

  /*
   * What the native selection holds: the keys it gave or was given, and the mode.
   */
  const syncedRef = useRef<{
    keys: ReadonlyArray<string> | null;
    multiple: boolean;
  }>({ keys: null, multiple: false });

  /*
   * The store with its selection holding the keys rendered now, or null without the store.
   */
  const nativeSelection = useCallback((): ListStore | null => {
    const current = storeRef.current;
    if (current === null) return null;
    const state = selectionStateRef.current;
    const synced = syncedRef.current;
    if (synced.keys !== state.keys || synced.multiple !== state.multiple) {
      current.module.setSelection(current.handle, state.keys, state.multiple);
      synced.keys = state.keys;
      synced.multiple = state.multiple;
    }
    return current;
  }, []);

  const applySelection = useCallback(
    (next: ReadonlyArray<string> | null | undefined) => {
      if (!next) return;
      const state = selectionStateRef.current;
      state.keys = next;
      syncedRef.current.keys = next;
      if (!state.controlled) setOwnSelectedKeys(next);
      state.onSelectionChange?.([...next]);
    },
    []
  );

  const selection = useMemo<RowSelection>(
    () => ({
      select: (key: string) => {
        if (!selectionStateRef.current.canSelect(key)) return;
        const native = nativeSelection();
        if (native) applySelection(native.module.selectKey(native.handle, key));
      },
      deselect: (key: string) => {
        const native = nativeSelection();
        if (native) {
          applySelection(native.module.deselectKey(native.handle, key));
        }
      },
      selectOnly: (key: string) => {
        if (!selectionStateRef.current.canSelect(key)) return;
        const native = nativeSelection();
        if (native) {
          applySelection(native.module.selectOnlyKey(native.handle, key));
        }
      },
    }),
    [applySelection, nativeSelection]
  );

  /*
   * A deleted row leaves the selection. Nothing is looked up while nothing is selected, and
   * native looks up only the selected keys.
   */
  const keysIdentity = keysIdentityOf(source);
  useEffect(() => {
    if (selectionStateRef.current.keys.length === 0) return;
    const version = sourceRef.current.keysVersion;
    if (version === 0) return;
    const native = nativeSelection();
    if (native) {
      applySelection(native.module.retainSelection(native.handle, version));
    }
  }, [keysIdentity, applySelection, nativeSelection]);

  const selectedIndices = useCallback((): number[] => {
    const version = sourceRef.current.keysVersion;
    const native = version > 0 ? nativeSelection() : null;
    return native ? native.module.selectedIndices(native.handle, version) : [];
  }, [nativeSelection]);

  return { selectedKeySet, selection, selectionStateRef, selectedIndices };
}
