import { useMemo, useRef } from 'react';
import type { RowSource } from './rowSource';

interface RowSpans {
  rowSpans: number[];
  rowSpansVersion: number;
}

/* The complete source snapshot, rather than only mounted rows, reaches native placement. */
export function useRowSpans<ItemT>(
  source: RowSource<ItemT>,
  getItemSpan: ((item: ItemT, index: number) => number) | undefined,
  columns: number,
  extraData: unknown
): RowSpans {
  const previous = useRef<RowSpans>({ rowSpans: [], rowSpansVersion: 0 });
  return useMemo(() => {
    const rowSpans: number[] = [];
    if (getItemSpan) {
      const count = Math.max(1, columns);
      for (let index = 0; index < source.count; index++) {
        const item = source.itemAt(index);
        const value = item === undefined ? 1 : getItemSpan(item, index);
        rowSpans.push(
          Number.isFinite(value)
            ? Math.min(count, Math.max(1, Math.trunc(value)))
            : 1
        );
      }
    }
    const previousSpans = previous.current;
    if (
      previousSpans.rowSpans.length === rowSpans.length &&
      previousSpans.rowSpans.every((span, index) => span === rowSpans[index])
    ) {
      return previousSpans;
    }
    previous.current = {
      rowSpans,
      rowSpansVersion: previousSpans.rowSpansVersion + 1,
    };
    return previous.current;
    // extraData refreshes a provider whose external state changed without a new callback.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [source, getItemSpan, columns, extraData]);
}
