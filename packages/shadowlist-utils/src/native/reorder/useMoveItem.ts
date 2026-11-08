import { useCallback, useLayoutEffect, useRef } from 'react';

type OnMoveItem<ItemT> = (info: {
  sourceIndex: number;
  destinationIndex: number;
  data: ItemT[];
}) => void;

/*
 * Moves an item one place for screen readers, reading data at action time so renderers keep
 * their identity.
 */
export function useMoveItem<ItemT extends { id: string }>(
  data: ReadonlyArray<ItemT>,
  onMoveItem: OnMoveItem<ItemT> | undefined
) {
  const latest = useRef({ data, onMoveItem });
  useLayoutEffect(() => {
    latest.current = { data, onMoveItem };
  });

  return useCallback((key: string, offset: -1 | 1) => {
    const { data: current, onMoveItem: moveItem } = latest.current;
    const sourceIndex = current.findIndex((item) => item.id === key);
    const destinationIndex = sourceIndex + offset;
    if (
      moveItem === undefined ||
      sourceIndex === -1 ||
      destinationIndex < 0 ||
      destinationIndex >= current.length
    ) {
      return;
    }
    const next = [...current];
    next.splice(destinationIndex, 0, ...next.splice(sourceIndex, 1));
    moveItem({ sourceIndex, destinationIndex, data: next });
  }, []);
}
