import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  type ComponentRef,
  type RefObject,
} from 'react';
import { processColor } from 'react-native';
import ShadowListCellView, {
  Commands as CellCommands,
  type NativeMenuAction,
  type NativeSwipeAction,
  type OnContextMenuAction,
  type OnSwipeAction,
} from '../ShadowListCellViewNativeComponent';
import type { ContextMenu, SwipeActionsConfiguration } from '../types';

type CellViewRef = RefObject<ComponentRef<typeof ShadowListCellView> | null>;

interface CellActionsOptions<ItemT> {
  item: ItemT;
  currentIndex: () => number;
  leadingSwipe: SwipeActionsConfiguration<ItemT> | null | undefined;
  trailingSwipe: SwipeActionsConfiguration<ItemT> | null | undefined;
  menu: ContextMenu<ItemT> | null | undefined;
  cellViewRef: CellViewRef;
}

/*
 * The swipe and menu props of a cell view.
 */
export interface CellActionProps {
  leadingSwipeActions: NativeSwipeAction[] | undefined;
  trailingSwipeActions: NativeSwipeAction[] | undefined;
  leadingFullSwipe: boolean | undefined;
  trailingFullSwipe: boolean | undefined;
  contextMenuTitle: string | undefined;
  contextMenuActions: NativeMenuAction[] | undefined;
  onSwipeAction: ((event: { nativeEvent: OnSwipeAction }) => void) | undefined;
  onContextMenuAction:
    | ((event: { nativeEvent: OnContextMenuAction }) => void)
    | undefined;
}

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

/*
 * Turns swipe actions and a menu into a cell view's props, and runs an action's onPress when
 * native reports it. Handlers get the item and the index it has when they run.
 */
export function useCellActions<ItemT>({
  item,
  currentIndex,
  leadingSwipe,
  trailingSwipe,
  menu,
  cellViewRef,
}: CellActionsOptions<ItemT>): CellActionProps {
  const nativeLeading = useMemo(
    () => nativeSwipeActions(leadingSwipe),
    [leadingSwipe]
  );
  const nativeTrailing = useMemo(
    () => nativeSwipeActions(trailingSwipe),
    [trailingSwipe]
  );
  const nativeMenu = useMemo(() => nativeMenuActions(menu), [menu]);

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
    [leadingSwipe, trailingSwipe, item, currentIndex, cellViewRef]
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

  const hasSwipe = nativeLeading !== undefined || nativeTrailing !== undefined;

  return {
    leadingSwipeActions: nativeLeading,
    trailingSwipeActions: nativeTrailing,
    leadingFullSwipe: nativeLeading
      ? (leadingSwipe?.performsFirstActionWithFullSwipe ?? true)
      : undefined,
    trailingFullSwipe: nativeTrailing
      ? (trailingSwipe?.performsFirstActionWithFullSwipe ?? true)
      : undefined,
    contextMenuTitle: menu?.title,
    contextMenuActions: nativeMenu,
    onSwipeAction: hasSwipe ? handleSwipeAction : undefined,
    onContextMenuAction: nativeMenu ? handleContextMenuAction : undefined,
  };
}
