import type { ReactElement } from 'react';
import type { ShadowListProps } from '../types';

/*
 * Default extra rows mounted on each side of the screen. See mountOverscanRows for how to pick one.
 */
export const SHADOWLIST_OVERSCAN = 4;

/*
 * Default extra rows mounted ahead in the scroll direction, overridden by mountOverscanRowsLeading.
 * A blank cell in a fling is a row native reached before React mounted it. Rows ahead are
 * what help. Rows behind only cost render work. A fling mounts this many ahead and
 * SHADOWLIST_OVERSCAN behind, and a list at rest keeps SHADOWLIST_OVERSCAN on both sides.
 */
export const SHADOWLIST_OVERSCAN_LEADING = 10;

/*
 * Maps snapAlignment to the native enum value.
 */
export const SNAP_ALIGNMENT = { start: 0, center: 1, end: 2 } as const;

/*
 * Device trace. Debug Apple builds started with SHADOWLIST_FRAME_TRACE=1 install
 * __shadowlistTrace, see ShadowListTrace.h. It prints [SLJ] lines on the same clock as the
 * native [SLF] frame trace. Elsewhere it's missing and a call costs one global read.
 * Only build messages behind slTraceEnabled().
 */
interface TraceGlobal {
  __shadowlistTrace?: (message: string) => void;
}

export function slTraceEnabled(): boolean {
  return typeof (globalThis as TraceGlobal).__shadowlistTrace === 'function';
}

export function slTrace(message: string): void {
  (globalThis as TraceGlobal).__shadowlistTrace?.(message);
}

/*
 * Milliseconds on a steady clock, for trace durations.
 */
export function slTraceNow(): number {
  return (
    (globalThis as { performance?: { now(): number } }).performance?.now() ??
    Date.now()
  );
}

/*
 * Rows rendered since the last commit trace line. All lists share this counter.
 */
let rowRenderCount = 0;

export function countRowRender(): void {
  if (slTraceEnabled()) rowRenderCount++;
}

export function takeRowRenderCount(): number {
  const count = rowRenderCount;
  rowRenderCount = 0;
  return count;
}

/*
 * The native view's tag, which is the id= in its [SLF] lines, or -1 before it mounts.
 */
export function nativeTagOf(instance: unknown): number {
  return (instance as { __nativeTag?: number } | null)?.__nativeTag ?? -1;
}

/*
 * The key of a row when the list has no keyExtractor: the item's id when
 * it is a string or a number, and its index otherwise. An index key belongs to the position,
 * not the row. Rows inserted above the screen then change the keys of the rows on it, and the
 * list cannot keep them still. Give such data a keyExtractor.
 */
export function defaultKeyExtractor(item: unknown, index: number): string {
  if (typeof item === 'object' && item !== null) {
    const id = (item as { id?: unknown }).id;
    if (typeof id === 'string' || typeof id === 'number') {
      return String(id);
    }
  }
  return String(index);
}

/*
 * The React element of a slot prop that takes one or a function returning one.
 */
export function renderComponent(
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
}

/*
 * How next differs from previous at the edges. pre counts rows added before the previous
 * first row, app after the previous last row, and mid in between, negative for removals. replace means
 * an edge row is gone.
 */
export function describeDataChange(
  previous: KeyedRows | null,
  next: KeyedRows
): string {
  if (previous === null) return `init:${next.count}`;
  if (previous.count === 0 || next.count === 0) {
    return `replace:${previous.count}->${next.count}`;
  }
  const firstKey = previous.keyAt(0);
  const lastKey = previous.keyAt(previous.count - 1);
  let firstIndex = -1;
  let lastIndex = -1;
  for (let index = 0; index < next.count; index++) {
    const key = next.keyAt(index);
    if (firstIndex < 0 && key === firstKey) firstIndex = index;
    if (key === lastKey) lastIndex = index;
  }
  if (firstIndex < 0 || lastIndex < 0) {
    return `replace:${previous.count}->${next.count}`;
  }
  const middle = lastIndex - firstIndex + 1 - previous.count;
  return `pre:${firstIndex} app:${next.count - 1 - lastIndex}${middle !== 0 ? ` mid:${middle}` : ''}`;
}

/*
 * Rows by count and key, for describeDataChange.
 */
interface KeyedRows {
  count: number;
  keyAt(index: number): string | undefined;
}

/*
 * Move one item, returning a new array.
 */
export function arrayMove<ItemT>(
  input: ReadonlyArray<ItemT>,
  from: number,
  to: number
): ItemT[] {
  const next = input.slice();
  if (
    from < 0 ||
    from >= next.length ||
    to < 0 ||
    to >= next.length ||
    from === to
  ) {
    return next;
  }
  const moved = next.splice(from, 1)[0] as ItemT;
  next.splice(to, 0, moved);
  return next;
}

/*
 * Sorted indices plus extra sorted indices, without duplicates, in one pass. Returns indices
 * itself when it already holds every extra one. A memo keyed on it then keeps its value.
 */
export function mergeSortedIndices(
  indices: number[],
  extra: ReadonlyArray<number>
): number[] {
  let missing = false;
  let position = 0;
  for (const index of extra) {
    while (position < indices.length && indices[position]! < index) position++;
    if (indices[position] !== index) {
      missing = true;
      break;
    }
  }
  if (!missing) return indices;
  const merged: number[] = [];
  let left = 0;
  let right = 0;
  while (left < indices.length || right < extra.length) {
    const next =
      right >= extra.length ||
      (left < indices.length && indices[left]! <= extra[right]!)
        ? indices[left++]!
        : extra[right++]!;
    if (merged[merged.length - 1] !== next) merged.push(next);
  }
  return merged;
}

/*
 * The ShadowList props that ShadowListForwardedProps leaves out. SectionList and TreeList drop
 * them when an untyped caller passes them, since they point at rows the caller never sees.
 */
const UNFORWARDED_PROPS: ReadonlyArray<keyof ShadowListProps<unknown>> = [
  'data',
  'keyExtractor',
  'stickyIndices',
  'renderStickyHeaderOverlay',
  'reorderEnabled',
  'onMoveItem',
  'dragBoardId',
  'dragListId',
  'dragListLabel',
  'onMoveItemToList',
  'viewabilityConfig',
  'onViewableItemsChanged',
  'viewabilityConfigCallbackPairs',
  'allowsMultipleSelection',
  'selectedKeys',
  'onSelectionChange',
  'shouldSelectItem',
  'enableSelectionOnKeyPress',
  'onActivateItem',
  'leadingSwipeActionsForItem',
  'trailingSwipeActionsForItem',
  'contextMenuForItem',
  'prefetchDataSource',
];

/*
 * The props already warned about, once per list type and prop.
 */
const warnedUnforwardedProps = new Set<string>();

/*
 * props without the ones SectionList and TreeList do not forward. Returns props itself when it
 * holds none of them.
 */
export function forwardedListProps<PropsT extends object>(
  props: PropsT,
  listName: string
): PropsT {
  let forwarded: Record<string, unknown> | null = null;
  for (const name of UNFORWARDED_PROPS) {
    if (!(name in props)) continue;
    if (__DEV__ && !warnedUnforwardedProps.has(`${listName}.${name}`)) {
      warnedUnforwardedProps.add(`${listName}.${name}`);
      console.warn(
        `ShadowList: ${listName} does not support ${name}. The prop is ignored.`
      );
    }
    forwarded ??= { ...props } as Record<string, unknown>;
    delete forwarded[name];
  }
  return (forwarded as PropsT | null) ?? props;
}
