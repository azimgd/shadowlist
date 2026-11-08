import {
  useCallback,
  useLayoutEffect,
  useRef,
  useState,
  type Dispatch,
  type SetStateAction,
} from 'react';
import { defaultBoardKeyExtractor, moveBoardItem } from './moveBoardItem';
import type { BoardColumn, BoardMoveInfo } from './types';

export interface UseDragBoardStateOptions<ItemT> {
  keyExtractor?: (item: ItemT) => string;
  onMoveItem?: (info: BoardMoveInfo<ItemT>) => void;
}

export interface DragBoardState<ItemT> {
  columns: ReadonlyArray<BoardColumn<ItemT>>;
  setColumns: Dispatch<SetStateAction<ReadonlyArray<BoardColumn<ItemT>>>>;
  moveItem: (info: BoardMoveInfo<ItemT>) => void;
}

/*
 * Holds a board's columns and applies each move to them. Pass columns and moveItem to
 * Board.List. moveItem keeps its identity across renders and updates every column a move
 * touches in one state change, which a drop between lists needs. onMoveItem hears about each
 * move that changes the board once that state change is queued, before the board renders it,
 * to save it or play a haptic. It is read when called and doesn't need to be stable. setColumns replaces the columns, for example after a refetch.
 */
export function useDragBoardState<ItemT>(
  initialColumns:
    | ReadonlyArray<BoardColumn<ItemT>>
    | (() => ReadonlyArray<BoardColumn<ItemT>>),
  options: UseDragBoardStateOptions<ItemT> = {}
): DragBoardState<ItemT> {
  const [columns, setColumnsState] = useState(initialColumns);
  const queued = useRef(columns);

  const latest = useRef(options);
  useLayoutEffect(() => {
    latest.current = options;
  });

  const setColumns = useCallback<
    Dispatch<SetStateAction<ReadonlyArray<BoardColumn<ItemT>>>>
  >((action) => {
    queued.current =
      typeof action === 'function' ? action(queued.current) : action;
    setColumnsState(queued.current);
  }, []);

  const moveItem = useCallback(
    (info: BoardMoveInfo<ItemT>) => {
      const { keyExtractor = defaultBoardKeyExtractor, onMoveItem } =
        latest.current;
      const next = moveBoardItem(queued.current, info, keyExtractor);
      if (next === queued.current) {
        return;
      }
      setColumns(next);
      onMoveItem?.(info);
    },
    [setColumns]
  );

  return { columns, setColumns, moveItem };
}
