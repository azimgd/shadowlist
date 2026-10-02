import { useCallback, useLayoutEffect, useRef } from 'react';

type OnReorder<ItemT> = (info: {
  from: number;
  to: number;
  data: ItemT[];
}) => void;

/*
 * Moves an item one place for screen readers, reading data at action time so renderers keep
 * their identity.
 */
export function useMoveItem<ItemT extends { id: string }>(
  data: ReadonlyArray<ItemT>,
  onReorder: OnReorder<ItemT> | undefined
) {
  const latest = useRef({ data, onReorder });
  useLayoutEffect(() => {
    latest.current = { data, onReorder };
  });

  return useCallback((id: string, offset: -1 | 1) => {
    const { data: current, onReorder: reorder } = latest.current;
    const from = current.findIndex((item) => item.id === id);
    const to = from + offset;
    if (
      reorder === undefined ||
      from === -1 ||
      to < 0 ||
      to >= current.length
    ) {
      return;
    }
    const next = [...current];
    next.splice(to, 0, ...next.splice(from, 1));
    reorder({ from, to, data: next });
  }, []);
}
