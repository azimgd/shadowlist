import type { Ref, ReactElement } from 'react';
import {
  useMemo,
  useState,
  useRef,
  useCallback,
  useImperativeHandle,
  forwardRef,
} from 'react';
import ShadowList from './ShadowList';
import type {
  ShadowListCommands,
  TreeListProps,
  TreeListCommands,
} from './types';

/*
 * TreeList sits on top of ShadowList, like SectionList does for sections. It flattens the
 * nodes whose parents are all expanded into one list and skips collapsed branches. Rows
 * are keyed by node id so they survive an expand or collapse.
 */

interface TreeFlatRow<ElementT> {
  id: string;
  element: ElementT;
  depth: number;
  hasChildren: boolean;
  isExpanded: boolean;
}

/*
 * Whether the row built for this position matches the mounted one. Everything renderRow
 * reads has to match, or a reused row would show old content.
 */
function sameRow<ElementT>(
  previous: TreeFlatRow<ElementT> | undefined,
  next: TreeFlatRow<ElementT>
): previous is TreeFlatRow<ElementT> {
  return (
    previous !== undefined &&
    previous.element === next.element &&
    previous.depth === next.depth &&
    previous.hasChildren === next.hasChildren &&
    previous.isExpanded === next.isExpanded
  );
}

function toSet(
  ids: ReadonlyArray<string> | ReadonlySet<string> | undefined
): Set<string> {
  if (!ids) return new Set();
  return ids instanceof Set ? new Set(ids) : new Set(ids as Iterable<string>);
}

// Returned when the inner list isn't mounted, so callers always get a map.
const EMPTY_SIZES: ReadonlyMap<string, number> = new Map();

function TreeListInner<ElementT>(
  {
    data,
    getChildren,
    keyExtractor,
    renderElement,
    expandedIds,
    initialExpandedIds,
    onExpandedChange,
    indentWidth = 16,
    getElementSizeSpec,
    ...rest
  }: TreeListProps<ElementT>,
  ref: Ref<TreeListCommands>
) {
  const innerRef = useRef<ShadowListCommands>(null);

  const isControlled = expandedIds !== undefined;
  const [internalExpanded, setInternalExpanded] = useState<Set<string>>(() =>
    toSet(initialExpandedIds)
  );
  const expandedSet = useMemo(
    () => (isControlled ? toSet(expandedIds) : internalExpanded),
    [isControlled, expandedIds, internalExpanded]
  );

  // Mirror the expanded set in a ref so two toggles in the same tick don't overwrite each other.
  const expandedRef = useRef(expandedSet);
  expandedRef.current = expandedSet;

  /*
   * Rows from the last flatten, by id, so an unchanged row keeps its old object. The list
   * mounts rows by identity, so without this an expand or collapse re-renders every row.
   */
  const previousRowsRef = useRef<Map<string, TreeFlatRow<ElementT>>>(new Map());

  const { data: rows, indexByKey } = useMemo(() => {
    const flat: TreeFlatRow<ElementT>[] = [];
    const byKey = new Map<string, number>();
    const previousRows = previousRowsRef.current;
    const nextRows = new Map<string, TreeFlatRow<ElementT>>();

    interface Frame {
      element: ElementT;
      depth: number;
    }
    const stack: Frame[] = [];
    for (let index = data.length - 1; index >= 0; index--) {
      stack.push({ element: data[index] as ElementT, depth: 0 });
    }

    while (stack.length > 0) {
      const { element, depth } = stack.pop() as Frame;
      const id = keyExtractor(element);
      const children = getChildren(element);
      const hasChildren = !!children && children.length > 0;
      const isExpanded = hasChildren && expandedSet.has(id);

      byKey.set(id, flat.length);
      const row = { id, element, depth, hasChildren, isExpanded };
      const previousRow = previousRows.get(id);
      const finalRow = sameRow(previousRow, row) ? previousRow : row;
      nextRows.set(id, finalRow);
      flat.push(finalRow);

      if (isExpanded && children) {
        for (let index = children.length - 1; index >= 0; index--) {
          stack.push({
            element: children[index] as ElementT,
            depth: depth + 1,
          });
        }
      }
    }

    previousRowsRef.current = nextRows;

    return { data: flat, indexByKey: byKey };
  }, [data, getChildren, keyExtractor, expandedSet]);

  const toggleId = useCallback(
    (id: string) => {
      const next = new Set(expandedRef.current);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      expandedRef.current = next;
      if (!isControlled) setInternalExpanded(next);
      onExpandedChange?.(next);
    },
    [isControlled, onExpandedChange]
  );

  useImperativeHandle(
    ref,
    () => ({
      setStartReachedEnabled: (enabled: boolean) =>
        innerRef.current?.setStartReachedEnabled(enabled),
      setEndReachedEnabled: (enabled: boolean) =>
        innerRef.current?.setEndReachedEnabled(enabled),
      scrollToIndex: (index: number, viewPosition?: number) =>
        innerRef.current?.scrollToIndex(index, viewPosition),
      scrollToOffset: (offset: number, animated?: boolean) =>
        innerRef.current?.scrollToOffset(offset, animated),
      scrollToEnd: (animated?: boolean) =>
        innerRef.current?.scrollToEnd(animated),
      scrollToNode: (id: string, viewPosition?: number) => {
        const index = indexByKey.get(id);
        if (index !== undefined) {
          innerRef.current?.scrollToIndex(index, viewPosition);
        }
      },
      getElementSize: (key: string) => innerRef.current?.getElementSize(key),
      getElementSizes: () => innerRef.current?.getElementSizes() ?? EMPTY_SIZES,
    }),
    [indexByKey]
  );

  /*
   * Pass one row to renderElement with its depth, indent, expanded state and a toggle.
   * index is a getter because the list only re-renders a moved row if it read the index.
   * Reading it here would re-render every row below an expand.
   */
  const renderRow = useCallback(
    (info: { element: TreeFlatRow<ElementT>; index: number }) => {
      const row = info.element;
      return renderElement({
        element: row.element,
        get index() {
          return info.index;
        },
        depth: row.depth,
        isExpanded: row.isExpanded,
        hasChildren: row.hasChildren,
        indent: row.depth * indentWidth,
        toggle: () => toggleId(row.id),
      });
    },
    [renderElement, indentWidth, toggleId]
  );

  const getRowSizeSpec = useMemo(
    () =>
      getElementSizeSpec
        ? (row: TreeFlatRow<ElementT>, index: number) =>
            getElementSizeSpec(row.element, index, row.depth)
        : undefined,
    [getElementSizeSpec]
  );

  return (
    <ShadowList
      {...rest}
      ref={innerRef}
      data={rows}
      renderElement={renderRow}
      getElementSizeSpec={getRowSizeSpec}
    />
  );
}

const TreeList = forwardRef(TreeListInner) as <ElementT>(
  props: TreeListProps<ElementT> & { ref?: Ref<TreeListCommands> }
) => ReactElement;

export default TreeList;
