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
 * move once that state change is queued, before the board renders it, to save it or play a
 * haptic. It is read when called and doesn't need to be stable. setColumns replaces the columns, for example after a refetch.
 */
export function useDragBoardState<ItemT>(
  initialColumns:
    | ReadonlyArray<BoardColumn<ItemT>>
    | (() => ReadonlyArray<BoardColumn<ItemT>>),
  options: UseDragBoardStateOptions<ItemT> = {}
): DragBoardState<ItemT> {
  const [columns, setColumns] = useState(initialColumns);

  const latest = useRef(options);
  useLayoutEffect(() => {
    latest.current = options;
  });

  const moveItem = useCallback((info: BoardMoveInfo<ItemT>) => {
    const { keyExtractor = defaultBoardKeyExtractor, onMoveItem } =
      latest.current;
    setColumns((previous) => moveBoardItem(previous, info, keyExtractor));
    onMoveItem?.(info);
  }, []);

  return { columns, setColumns, moveItem };
}
