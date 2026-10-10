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
import ShadowListElementView, {
  Commands as ElementCommands,
  type NativeMenuAction,
  type NativeSwipeAction,
  type OnContextMenuAction,
  type OnSwipeAction,
} from '../ShadowListElementViewNativeComponent';
import type {
  ContextMenu,
  ItemSeparatorProps,
  RenderElementInfo,
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

type ItemInfo<ElementT> = { item: ElementT; index: number };

interface ElementRendererProps<ElementT> {
  element: ElementT;
  index: number;
  rowIndex: RowIndexStore;
  elementKey: string;
  style: StyleProp<ViewStyle>;
  renderElement: (info: RenderElementInfo<ElementT>) => ReactElement;
  separator: ReactElement | null;
  Separator: ComponentType<ItemSeparatorProps<ElementT>> | null;
  trailingElement: ElementT | undefined;
  separatorStore: SeparatorStore;
  selected: boolean;
  selection: RowSelection;
  leadingSwipeActionsForItem:
    | ((
        info: ItemInfo<ElementT>
      ) => SwipeActionsConfiguration<ElementT> | null | undefined)
    | undefined;
  trailingSwipeActionsForItem:
    | ((
        info: ItemInfo<ElementT>
      ) => SwipeActionsConfiguration<ElementT> | null | undefined)
    | undefined;
  contextMenuForItem:
    | ((info: ItemInfo<ElementT>) => ContextMenu<ElementT> | null | undefined)
    | undefined;
  nativeIndex: number;
  onElementLayout?: (key: string, width: number, height: number) => void;
  onElementRelease?: (key: string) => void;
}

interface RenderedChildren<ElementT> {
  element: ElementT;
  index: number;
  renderElement: ElementRendererProps<ElementT>['renderElement'];
  separator: ReactElement | null;
  Separator: ElementRendererProps<ElementT>['Separator'];
  trailingElement: ElementT | undefined;
  separatorState: SeparatorState;
  selected: boolean;
  readIndex: boolean;
  children: ReactElement;
}

/*
 * Props equal except maybe index, and the index only counts for a row that read it. A prepend
 * shifts every mounted row's index, and without this every row would run again just to hand the
 * same children back. Keep this in sync with ElementRendererProps.
 */
function sameRowProps<ElementT>(
  previous: ElementRendererProps<ElementT>,
  next: ElementRendererProps<ElementT>
): boolean {
  return (
    previous.element === next.element &&
    previous.elementKey === next.elementKey &&
    previous.style === next.style &&
    previous.renderElement === next.renderElement &&
    previous.separator === next.separator &&
    previous.Separator === next.Separator &&
    previous.trailingElement === next.trailingElement &&
    previous.separatorStore === next.separatorStore &&
    previous.selected === next.selected &&
    previous.selection === next.selection &&
    previous.leadingSwipeActionsForItem === next.leadingSwipeActionsForItem &&
    previous.trailingSwipeActionsForItem === next.trailingSwipeActionsForItem &&
    previous.contextMenuForItem === next.contextMenuForItem &&
    previous.nativeIndex === next.nativeIndex &&
    previous.onElementLayout === next.onElementLayout &&
    previous.onElementRelease === next.onElementRelease &&
    previous.rowIndex === next.rowIndex &&
    (previous.index === next.index ||
      !next.rowIndex.readers.has(next.elementKey))
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
function nativeSwipeActions<ElementT>(
  configuration: SwipeActionsConfiguration<ElementT> | null | undefined
): NativeSwipeAction[] | undefined {
  if (!configuration || configuration.actions.length === 0) return undefined;
  return configuration.actions.map((action) => {
    const destructive = action.style === 'destructive';
    const fallback = destructive ? '#FF3B30' : '#8E8E93';
    const color = processColor(action.backgroundColor ?? fallback);
    return {
      title: action.title,
      color:
        typeof color === 'number' ? color : (processColor(fallback) as number),
      destructive,
    };
  });
}

function nativeMenuActions<ElementT>(
  menu: ContextMenu<ElementT> | null | undefined
): NativeMenuAction[] | undefined {
  if (!menu || menu.actions.length === 0) return undefined;
  return menu.actions.map((action) => ({
    title: action.title,
    destructive: action.style === 'destructive',
    disabled: action.disabled ?? false,
    systemImage: action.systemImage ?? '',
  }));
}

export const ElementRenderer = memo(function ElementRendererInner<ElementT>({
  element,
  index,
  rowIndex,
  elementKey,
  style,
  renderElement,
  separator,
  Separator,
  trailingElement,
  separatorStore,
  selected,
  selection,
  leadingSwipeActionsForItem,
  trailingSwipeActionsForItem,
  contextMenuForItem,
  nativeIndex,
  onElementLayout,
  onElementRelease,
}: ElementRendererProps<ElementT>) {
  /*
   * This row's own separator, the one below it. Rows without a separator component never
   * subscribe.
   */
  const subscribe = useCallback(
    (listener: () => void) => separatorStore.subscribe(elementKey, listener),
    [separatorStore, elementKey]
  );
  const separatorState = useSyncExternalStore(
    Separator ? subscribe : NO_SUBSCRIPTION,
    () => separatorStore.get(elementKey)
  );

  /*
   * The getter returns the row's current index and marks it as read, even after render, like
   * from a press handler. Such a row re-renders on its next move. A late read looks the key
   * up, since a row that never read the index skips moves and its own index goes stale.
   */
  const indexRef = useRef(index);
  indexRef.current = index;
  const currentIndex = useCallback(
    () => rowIndex.keyToIndex.get(elementKey) ?? indexRef.current,
    [rowIndex, elementKey]
  );

  /*
   * The row's separators. Highlighting a row highlights the separators on both sides of it.
   * The one above belongs to the row before.
   */
  const separators = useMemo<Separators>(() => {
    const previousKey = () => rowIndex.keys[currentIndex() - 1];
    return {
      highlight: () => {
        separatorStore.setHighlighted(elementKey, true);
        const above = previousKey();
        if (above !== undefined) separatorStore.setHighlighted(above, true);
      },
      unhighlight: () => {
        separatorStore.setHighlighted(elementKey, false);
        const above = previousKey();
        if (above !== undefined) separatorStore.setHighlighted(above, false);
      },
      updateProps: (select, newProps) => {
        const key = select === 'leading' ? previousKey() : elementKey;
        if (key !== undefined) separatorStore.updateProps(key, newProps);
      },
    };
  }, [rowIndex, separatorStore, elementKey, currentIndex]);

  const select = useCallback(
    () => selection.select(elementKey),
    [selection, elementKey]
  );
  const deselect = useCallback(
    () => selection.deselect(elementKey),
    [selection, elementKey]
  );

  /*
   * Rebuild the row only when something it used changed. The index only counts if the last
   * renderElement call read it. A prepend shifts every row's index but not its element. A
   * row that never read the index can keep its children and React skips the subtree.
   * A renderer that does read the index, say for numbering, still re-renders when it moves.
   */
  const renderedRef = useRef<RenderedChildren<ElementT> | null>(null);
  const rendered = renderedRef.current;
  let children: ReactElement;
  if (
    rendered !== null &&
    rendered.element === element &&
    rendered.renderElement === renderElement &&
    rendered.separator === separator &&
    rendered.Separator === Separator &&
    rendered.trailingElement === trailingElement &&
    rendered.separatorState === separatorState &&
    rendered.selected === selected &&
    (!rendered.readIndex || rendered.index === index)
  ) {
    children = rendered.children;
  } else {
    countRowRender();
    if (rendered !== null && slTraceEnabled()) {
      slTrace(
        `row-miss key=${elementKey} element=${rendered.element !== element ? 1 : 0}` +
          ` render=${rendered.renderElement !== renderElement ? 1 : 0}` +
          ` separator=${rendered.separator !== separator || rendered.Separator !== Separator || rendered.separatorState !== separatorState ? 1 : 0}` +
          ` index=${rendered.readIndex && rendered.index !== index ? 1 : 0}`
      );
    }
    const next: RenderedChildren<ElementT> = {
      element,
      index,
      renderElement,
      separator,
      Separator,
      trailingElement,
      separatorState,
      selected,
      readIndex: false,
      children: null as unknown as ReactElement,
    };
    let rendering = true;
    const content = renderElement({
      element,
      get index() {
        next.readIndex = true;
        rowIndex.readers.add(elementKey);
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
          leadingItem={element}
          trailingItem={trailingElement}
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
   * Swipe actions and the context menu, asked once per element. Their handlers get the row's
   * index when they run.
   */
  const leadingSwipe = useMemo(
    () =>
      leadingSwipeActionsForItem?.({ item: element, index: currentIndex() }),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [leadingSwipeActionsForItem, element]
  );
  const trailingSwipe = useMemo(
    () =>
      trailingSwipeActionsForItem?.({ item: element, index: currentIndex() }),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [trailingSwipeActionsForItem, element]
  );
  const menu = useMemo(
    () => contextMenuForItem?.({ item: element, index: currentIndex() }),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [contextMenuForItem, element]
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

  const elementViewRef = useRef<ComponentRef<
    typeof ShadowListElementView
  > | null>(null);
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
          item: element,
          index: currentIndex(),
        });
      } finally {
        if (fullSwipe) {
          const closeFullSwipe = () =>
            afterNextCommit(() => {
              const view = elementViewRef.current;
              if (mountedRef.current && view) {
                ElementCommands.closeFullSwipe(view);
              }
            });
          Promise.resolve(result).then(closeFullSwipe, closeFullSwipe);
        }
      }
    },
    [leadingSwipe, trailingSwipe, element, currentIndex]
  );

  const handleContextMenuAction = useCallback(
    (event: { nativeEvent: OnContextMenuAction }) => {
      menu?.actions[event.nativeEvent.actionIndex]?.onPress({
        item: element,
        index: currentIndex(),
      });
    },
    [menu, element, currentIndex]
  );

  const handleLayout = useCallback(
    (event: LayoutChangeEvent) => {
      const { width, height } = event.nativeEvent.layout;
      onElementLayout?.(elementKey, width, height);
    },
    [onElementLayout, elementKey]
  );

  /*
   * Reader marks are only added during render, since a thrown away render could otherwise
   * clear the mark of the committed one. Unmount clears it, and a remount, like StrictMode's,
   * adds it back.
   */
  useEffect(() => {
    if (renderedRef.current?.readIndex) rowIndex.readers.add(elementKey);
    return () => {
      rowIndex.readers.delete(elementKey);
    };
  }, [rowIndex, elementKey]);

  // Forget the row's size on unmount. If the key comes back it gets measured again.
  useEffect(() => {
    if (!onElementRelease) return;
    return () => onElementRelease(elementKey);
  }, [onElementRelease, elementKey]);

  const hasSwipe = nativeLeading !== undefined || nativeTrailing !== undefined;

  return (
    <ShadowListElementView
      ref={elementViewRef}
      index={nativeIndex}
      elementKey={elementKey}
      style={style}
      onLayout={onElementLayout ? handleLayout : undefined}
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
    </ShadowListElementView>
  );
}, sameRowProps) as <ElementT>(
  props: ElementRendererProps<ElementT>
) => ReactElement;
