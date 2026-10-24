import {
  memo,
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useSyncExternalStore,
  type ComponentRef,
  type ComponentType,
  type ReactElement,
} from 'react';
import type { LayoutChangeEvent, StyleProp, ViewStyle } from 'react-native';
import ShadowListCellView from '../ShadowListCellViewNativeComponent';
import type {
  ContextMenu,
  ItemSeparatorProps,
  RenderItemInfo,
  Separators,
  SwipeActionsConfiguration,
} from '../types';
import { countRowRender, slTrace, slTraceEnabled } from './helpers';
import type { SeparatorState, SeparatorStore } from './separators';
import { EMPTY_SOURCE, type RowSource } from './rowSource';
import { useCellActions } from './useCellActions';

/*
 * Per list row index state shared by every row. Rows keep their children across moves, and
 * the memo check below skips a row whose only change is its index, unless the row read it.
 * source answers index reads made after render, like from a press handler, for rows that
 * skipped a move and still hold a stale index, and finds the key of the row above a row.
 */
export interface RowIndexStore {
  source: RowSource<unknown>;
  readers: Set<string>;
}

export function createRowIndexStore(): RowIndexStore {
  return { source: EMPTY_SOURCE, readers: new Set() };
}

/*
 * What a row does when selected or deselected, shared by every row. selectOnly leaves the
 * one row selected, like an arrow key does.
 */
export interface RowSelection {
  select: (key: string) => void;
  deselect: (key: string) => void;
  selectOnly: (key: string) => void;
}

type ItemInfo<ItemT> = { item: ItemT; index: number };

interface CellRendererProps<ItemT> {
  item: ItemT;
  index: number;
  rowIndex: RowIndexStore;
  rowKey: string;
  style: StyleProp<ViewStyle>;
  renderItem: (info: RenderItemInfo<ItemT>) => ReactElement;
  separator: ReactElement | null;
  Separator: ComponentType<ItemSeparatorProps<ItemT>> | null;
  trailingItem: ItemT | undefined;
  separatorStore: SeparatorStore;
  selected: boolean;
  selection: RowSelection;
  leadingSwipeActionsForItem:
    | ((
        info: ItemInfo<ItemT>
      ) => SwipeActionsConfiguration<ItemT> | null | undefined)
    | undefined;
  trailingSwipeActionsForItem:
    | ((
        info: ItemInfo<ItemT>
      ) => SwipeActionsConfiguration<ItemT> | null | undefined)
    | undefined;
  contextMenuForItem:
    | ((info: ItemInfo<ItemT>) => ContextMenu<ItemT> | null | undefined)
    | undefined;
  nativeIndex: number;
  onCellLayout?: (key: string, width: number, height: number) => void;
  onCellRelease?: (key: string) => void;
}

interface RenderedChildren<ItemT> {
  item: ItemT;
  index: number;
  renderItem: CellRendererProps<ItemT>['renderItem'];
  separator: ReactElement | null;
  Separator: CellRendererProps<ItemT>['Separator'];
  trailingItem: ItemT | undefined;
  separatorState: SeparatorState;
  selected: boolean;
  readIndex: boolean;
  children: ReactElement;
}

/*
 * Props equal except maybe index, and the index only counts for a row that read it. A prepend
 * shifts every mounted row's index, and without this every row would run again just to hand the
 * same children back. Keep this in sync with CellRendererProps.
 */
function sameRowProps<ItemT>(
  previous: CellRendererProps<ItemT>,
  next: CellRendererProps<ItemT>
): boolean {
  return (
    previous.item === next.item &&
    previous.rowKey === next.rowKey &&
    previous.style === next.style &&
    previous.renderItem === next.renderItem &&
    previous.separator === next.separator &&
    previous.Separator === next.Separator &&
    previous.trailingItem === next.trailingItem &&
    previous.separatorStore === next.separatorStore &&
    previous.selected === next.selected &&
    previous.selection === next.selection &&
    previous.leadingSwipeActionsForItem === next.leadingSwipeActionsForItem &&
    previous.trailingSwipeActionsForItem === next.trailingSwipeActionsForItem &&
    previous.contextMenuForItem === next.contextMenuForItem &&
    previous.nativeIndex === next.nativeIndex &&
    previous.onCellLayout === next.onCellLayout &&
    previous.onCellRelease === next.onCellRelease &&
    previous.rowIndex === next.rowIndex &&
    (previous.index === next.index || !next.rowIndex.readers.has(next.rowKey))
  );
}

const NO_SUBSCRIPTION = () => () => {};

export const CellRenderer = memo(function CellRendererInner<ItemT>({
  item,
  index,
  rowIndex,
  rowKey,
  style,
  renderItem,
  separator,
  Separator,
  trailingItem,
  separatorStore,
  selected,
  selection,
  leadingSwipeActionsForItem,
  trailingSwipeActionsForItem,
  contextMenuForItem,
  nativeIndex,
  onCellLayout,
  onCellRelease,
}: CellRendererProps<ItemT>) {
  /*
   * This row's own separator, the one below it. Rows without a separator component never
   * subscribe.
   */
  const subscribe = useCallback(
    (listener: () => void) => separatorStore.subscribe(rowKey, listener),
    [separatorStore, rowKey]
  );
  const separatorState = useSyncExternalStore(
    Separator ? subscribe : NO_SUBSCRIPTION,
    () => separatorStore.get(rowKey)
  );

  /*
   * The getter returns the row's current index and marks it as read, even after render, like
   * from a press handler. Such a row re-renders on its next move. A late read looks the key
   * up, since a row that never read the index skips moves and its own index goes stale.
   */
  const indexRef = useRef(index);
  indexRef.current = index;
  const currentIndex = useCallback(() => {
    // The row's own index is where to look first. A stale one is still close.
    const found = rowIndex.source.indexOfKey(rowKey, indexRef.current);
    return found >= 0 ? found : indexRef.current;
  }, [rowIndex, rowKey]);

  /*
   * The row's separators. Highlighting a row highlights the separators on both sides of it.
   * The one above belongs to the row before.
   */
  const separators = useMemo<Separators>(() => {
    const previousKey = () => rowIndex.source.keyAt(currentIndex() - 1);
    return {
      highlight: () => {
        separatorStore.setHighlighted(rowKey, true);
        const above = previousKey();
        if (above !== undefined) separatorStore.setHighlighted(above, true);
      },
      unhighlight: () => {
        separatorStore.setHighlighted(rowKey, false);
        const above = previousKey();
        if (above !== undefined) separatorStore.setHighlighted(above, false);
      },
      updateProps: (select, newProps) => {
        const key = select === 'leading' ? previousKey() : rowKey;
        if (key !== undefined) separatorStore.updateProps(key, newProps);
      },
    };
  }, [rowIndex, separatorStore, rowKey, currentIndex]);

  const select = useCallback(
    () => selection.select(rowKey),
    [selection, rowKey]
  );
  const deselect = useCallback(
    () => selection.deselect(rowKey),
    [selection, rowKey]
  );

  /*
   * Rebuild the row only when something it used changed. The index only counts if the last
   * renderItem call read it. A prepend shifts every row's index but not its item. A
   * row that never read the index can keep its children and React skips the subtree.
   * A renderer that does read the index, say for numbering, still re-renders when it moves.
   */
  const renderedRef = useRef<RenderedChildren<ItemT> | null>(null);
  const rendered = renderedRef.current;
  let children: ReactElement;
  if (
    rendered !== null &&
    rendered.item === item &&
    rendered.renderItem === renderItem &&
    rendered.separator === separator &&
    rendered.Separator === Separator &&
    rendered.trailingItem === trailingItem &&
    rendered.separatorState === separatorState &&
    rendered.selected === selected &&
    (!rendered.readIndex || rendered.index === index)
  ) {
    children = rendered.children;
  } else {
    countRowRender();
    if (rendered !== null && slTraceEnabled()) {
      slTrace(
        `row-miss key=${rowKey} item=${rendered.item !== item ? 1 : 0}` +
          ` render=${rendered.renderItem !== renderItem ? 1 : 0}` +
          ` separator=${rendered.separator !== separator || rendered.Separator !== Separator || rendered.separatorState !== separatorState ? 1 : 0}` +
          ` index=${rendered.readIndex && rendered.index !== index ? 1 : 0}`
      );
    }
    const next: RenderedChildren<ItemT> = {
      item,
      index,
      renderItem,
      separator,
      Separator,
      trailingItem,
      separatorState,
      selected,
      readIndex: false,
      children: null as unknown as ReactElement,
    };
    let rendering = true;
    const content = renderItem({
      item,
      get index() {
        next.readIndex = true;
        rowIndex.readers.add(rowKey);
        if (rendering) return indexRef.current;
        return currentIndex();
      },
      separators,
      selected,
      select,
      deselect,
    });
    rendering = false;
    let rowSeparator = separator;
    if (Separator !== null) {
      rowSeparator = (
        <Separator
          highlighted={separatorState.highlighted}
          leadingItem={item}
          trailingItem={trailingItem}
          {...separatorState.props}
        />
      );
    }
    next.children = (
      <>
        {content}
        {rowSeparator}
      </>
    );
    renderedRef.current = next;
    children = next.children;
  }

  /*
   * Swipe actions and the context menu, asked once per item. Their handlers get the row's
   * index when they run.
   */
  const leadingSwipe = useMemo(
    () => leadingSwipeActionsForItem?.({ item, index: currentIndex() }),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [leadingSwipeActionsForItem, item]
  );
  const trailingSwipe = useMemo(
    () => trailingSwipeActionsForItem?.({ item, index: currentIndex() }),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [trailingSwipeActionsForItem, item]
  );
  const menu = useMemo(
    () => contextMenuForItem?.({ item, index: currentIndex() }),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [contextMenuForItem, item]
  );

  const cellViewRef = useRef<ComponentRef<typeof ShadowListCellView> | null>(
    null
  );
  const actionProps = useCellActions({
    item,
    currentIndex,
    leadingSwipe,
    trailingSwipe,
    menu,
    cellViewRef,
  });

  const handleLayout = useCallback(
    (event: LayoutChangeEvent) => {
      const { width, height } = event.nativeEvent.layout;
      onCellLayout?.(rowKey, width, height);
    },
    [onCellLayout, rowKey]
  );

  /*
   * Reader marks are only added during render, since a thrown away render could otherwise
   * clear the mark of the committed one. Unmount clears it, and a remount, like StrictMode's,
   * adds it back.
   */
  useEffect(() => {
    if (renderedRef.current?.readIndex) rowIndex.readers.add(rowKey);
    return () => {
      rowIndex.readers.delete(rowKey);
    };
  }, [rowIndex, rowKey]);

  // Forget the row's size on unmount. If the key comes back it gets measured again.
  useEffect(() => {
    if (!onCellRelease) return;
    return () => onCellRelease(rowKey);
  }, [onCellRelease, rowKey]);

  return (
    <ShadowListCellView
      ref={cellViewRef}
      index={nativeIndex}
      rowKey={rowKey}
      style={style}
      onLayout={onCellLayout ? handleLayout : undefined}
      {...actionProps}
    >
      {children}
    </ShadowListCellView>
  );
}, sameRowProps) as <ItemT>(props: CellRendererProps<ItemT>) => ReactElement;
