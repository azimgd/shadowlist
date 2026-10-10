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
import {
  processColor,
  type LayoutChangeEvent,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import ShadowListCellView, {
  Commands as CellCommands,
  type NativeMenuAction,
  type NativeSwipeAction,
  type OnContextMenuAction,
  type OnSwipeAction,
} from '../ShadowListCellViewNativeComponent';
import type {
  ContextMenu,
  ItemSeparatorProps,
  RenderItemInfo,
  Separators,
  SwipeActionsConfiguration,
} from '../types';
import { countRowRender, slTrace, slTraceEnabled } from './helpers';
import type { SeparatorState, SeparatorStore } from './separators';

/*
 * Per list row index state shared by every row. Rows keep their children across moves, and
 * the memo check below skips a row whose only change is its index, unless the row read it.
 * keyToIndex answers index reads made after render, like from a press handler, for rows
 * that skipped a move and still hold a stale index. keys finds the row above a row.
 */
export interface RowIndexStore {
  keyToIndex: ReadonlyMap<string, number>;
  keys: ReadonlyArray<string>;
  readers: Set<string>;
}

export function createRowIndexStore(): RowIndexStore {
  return { keyToIndex: new Map(), keys: [], readers: new Set() };
}

/*
 * What a row does when selected or deselected, shared by every row.
 */
export interface RowSelection {
  select: (key: string) => void;
  deselect: (key: string) => void;
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

/*
 * Calls back after the next macrotask and two frames. A row removed by the app's handler, even
 * through a store that notifies on a timer, has unmounted by then.
 */
function afterNextCommit(callback: () => void) {
  setTimeout(() => {
    requestAnimationFrame(() => requestAnimationFrame(callback));
  }, 0);
}

/*
 * Swipe action buttons for native, with the default colors of the native lists: red for a
 * destructive action and gray otherwise.
 */
function nativeSwipeActions<ItemT>(
  configuration: SwipeActionsConfiguration<ItemT> | null | undefined
): NativeSwipeAction[] | undefined {
  if (!configuration || configuration.actions.length === 0) return undefined;
  return configuration.actions.map((action) => {
    const style = action.style ?? 'normal';
    const fallback = style === 'destructive' ? '#FF3B30' : '#8E8E93';
    const backgroundColor = processColor(action.backgroundColor ?? fallback);
    return {
      title: action.title,
      backgroundColor:
        typeof backgroundColor === 'number'
          ? backgroundColor
          : (processColor(fallback) as number),
      style,
    };
  });
}

function nativeMenuActions<ItemT>(
  menu: ContextMenu<ItemT> | null | undefined
): NativeMenuAction[] | undefined {
  if (!menu || menu.actions.length === 0) return undefined;
  return menu.actions.map((action) => ({
    title: action.title,
    style: action.style ?? 'normal',
    disabled: action.disabled ?? false,
    systemImage: action.systemImage ?? '',
  }));
}

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
  const currentIndex = useCallback(
    () => rowIndex.keyToIndex.get(rowKey) ?? indexRef.current,
    [rowIndex, rowKey]
  );

  /*
   * The row's separators. Highlighting a row highlights the separators on both sides of it.
   * The one above belongs to the row before.
   */
  const separators = useMemo<Separators>(() => {
    const previousKey = () => rowIndex.keys[currentIndex() - 1];
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
  const nativeLeading = useMemo(
    () => nativeSwipeActions(leadingSwipe),
    [leadingSwipe]
  );
  const nativeTrailing = useMemo(
    () => nativeSwipeActions(trailingSwipe),
    [trailingSwipe]
  );
  const nativeMenu = useMemo(() => nativeMenuActions(menu), [menu]);

  const cellViewRef = useRef<ComponentRef<typeof ShadowListCellView> | null>(
    null
  );
  const mountedRef = useRef(false);
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  /*
   * A full swipe leaves the row slid out while its action runs. A handler that returns a promise
   * keeps it out until the promise settles. A row still here after that slides back. One the
   * action removed has unmounted and goes with its data.
   */
  const handleSwipeAction = useCallback(
    (event: { nativeEvent: OnSwipeAction }) => {
      const { leading, actionIndex, fullSwipe } = event.nativeEvent;
      const configuration = leading ? leadingSwipe : trailingSwipe;
      let result: unknown;
      try {
        result = configuration?.actions[actionIndex]?.onPress({
          item,
          index: currentIndex(),
        });
      } finally {
        if (fullSwipe) {
          const closeFullSwipe = () =>
            afterNextCommit(() => {
              const view = cellViewRef.current;
              if (mountedRef.current && view) {
                CellCommands.closeFullSwipe(view);
              }
            });
          Promise.resolve(result).then(closeFullSwipe, closeFullSwipe);
        }
      }
    },
    [leadingSwipe, trailingSwipe, item, currentIndex]
  );

  const handleContextMenuAction = useCallback(
    (event: { nativeEvent: OnContextMenuAction }) => {
      menu?.actions[event.nativeEvent.actionIndex]?.onPress({
        item,
        index: currentIndex(),
      });
    },
    [menu, item, currentIndex]
  );

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

  const hasSwipe = nativeLeading !== undefined || nativeTrailing !== undefined;

  return (
    <ShadowListCellView
      ref={cellViewRef}
      index={nativeIndex}
      rowKey={rowKey}
      style={style}
      onLayout={onCellLayout ? handleLayout : undefined}
      leadingSwipeActions={nativeLeading}
      trailingSwipeActions={nativeTrailing}
      leadingFullSwipe={
        nativeLeading
          ? (leadingSwipe?.performsFirstActionWithFullSwipe ?? true)
          : undefined
      }
      trailingFullSwipe={
        nativeTrailing
          ? (trailingSwipe?.performsFirstActionWithFullSwipe ?? true)
          : undefined
      }
      contextMenuTitle={menu?.title}
      contextMenuActions={nativeMenu}
      onSwipeAction={hasSwipe ? handleSwipeAction : undefined}
      onContextMenuAction={nativeMenu ? handleContextMenuAction : undefined}
    >
      {children}
    </ShadowListCellView>
  );
}, sameRowProps) as <ItemT>(props: CellRendererProps<ItemT>) => ReactElement;
