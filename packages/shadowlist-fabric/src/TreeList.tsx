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
 * TreeList is a data layer over ShadowList, the tree analogue of SectionList. It
 * flattens the visible subtree (nodes whose ancestors are all expanded) into one
 * element stream; collapsed subtrees are never descended into. Each row keeps a
 * stable node-id key so surviving rows reconcile across an expand/collapse toggle.
 */

interface TreeFlatRow<ElementT> {
  id: string;
  element: ElementT;
  depth: number;
  hasChildren: boolean;
  isExpanded: boolean;
}

/*
 * Whether the row built for this position is the one already mounted: everything renderRow
 * reads from it has to match, or a reused row would show stale content.
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

  /* Mirror the expanded set into a ref so two toggles in the same tick build on each
   * other instead of overwriting one another. */
  const expandedRef = useRef(expandedSet);
  expandedRef.current = expandedSet;

  /*
   * The rows built by the previous flatten, by id, so an unchanged row keeps the object it
   * already had. The list mounts rows by identity: without this, an expand or collapse hands
   * every mounted row a new object and the whole window re-renders.
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
      scrollToIndex: (index: number) => innerRef.current?.scrollToIndex(index),
      scrollToOffset: (offset: number, animated?: boolean) =>
        innerRef.current?.scrollToOffset(offset, animated),
      scrollToEnd: (animated?: boolean) =>
        innerRef.current?.scrollToEnd(animated),
      scrollToNode: (id: string) => {
        const index = indexByKey.get(id);
        if (index !== undefined) innerRef.current?.scrollToIndex(index);
      },
    }),
    [indexByKey]
  );

  /*
   * Hand one flattened row to renderElement, adding tree info (depth, indent, whether it
   * has children and is expanded) and a toggle to expand/collapse that node. `index` is
   * forwarded as a getter: the list re-renders a row that moved only if its renderer read
   * the index, so reading it eagerly here would re-render every row below an expand.
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
