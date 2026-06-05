import {
  forwardRef,
  useCallback,
  useImperativeHandle,
  useMemo,
  useRef,
  useState,
  type ReactElement,
  type Ref,
} from 'react';
import Shadowlist from './Shadowlist.js';
import type {
  ShadowlistCommands,
  TreeListProps,
  TreeListCommands,
} from './types.js';

/*
 * TreeList is a thin data layer over Shadowlist, the tree analogue of SectionList.
 * It flattens the *visible* subtree - every node whose ancestors are all expanded -
 * into one tagged element stream and hands that to the virtualizer. Collapsed
 * subtrees are never descended into, so the flatten cost is proportional to the
 * number of visible rows, not the size of the whole tree: a million-node tree with
 * everything collapsed flattens to its root count.
 *
 * Expand/collapse is just "the flat key set changed". Each row keeps a stable key
 * (the node id), so Shadowlist's key-based reconcile preserves the measured height
 * of every surviving row while children appear/disappear below it. No engine change
 * is required - all the heavy lifting is the unchanged virtualizer.
 */

interface TreeFlatRow<ItemT> {
  /*
   * Stable key for the flattened row. Equals keyExtractor(item); Shadowlist keys on
   * element.id so surviving rows reconcile across a toggle without remeasuring.
   */
  id: string;
  item: ItemT;
  depth: number;
  hasChildren: boolean;
  isExpanded: boolean;
}

/*
 * Normalise the array|Set expansion inputs to a Set for O(1) membership tests
 * during flatten. A fresh Set is only built when the source identity changes.
 */
const toSet = (
  ids: ReadonlyArray<string> | ReadonlySet<string> | undefined
): Set<string> => {
  if (!ids) return new Set();
  return ids instanceof Set ? new Set(ids) : new Set(ids as Iterable<string>);
};

function TreeListInner<ItemT>(
  {
    data,
    getChildren,
    keyExtractor,
    renderNode,
    expandedIds,
    initialExpandedIds,
    onExpandedChange,
    indentWidth = 16,
    style,
    elementStyle,
    initialElementsSize,
    containerOffsetIndex,
    onScroll,
    onStartReached,
    onEndReached,
    onStartReachedThreshold,
    onEndReachedThreshold,
    ItemSeparatorComponent,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
  }: TreeListProps<ItemT>,
  ref: Ref<TreeListCommands>
) {
  const innerRef = useRef<ShadowlistCommands>(null);

  /*
   * Controlled when expandedIds is provided; otherwise the list owns the set. The
   * uncontrolled state is seeded once from initialExpandedIds.
   */
  const isControlled = expandedIds !== undefined;
  const [internalExpanded, setInternalExpanded] = useState<Set<string>>(() =>
    toSet(initialExpandedIds)
  );
  const expandedSet = useMemo(
    () => (isControlled ? toSet(expandedIds) : internalExpanded),
    [isControlled, expandedIds, internalExpanded]
  );

  /*
   * The current expansion, kept in a ref so a toggle composes on top of the most
   * recent set rather than the prop captured at render time. In controlled mode the
   * prop only catches up on the parent's next render, so two toggles dispatched in
   * one tick must both build on this ref or the second would clobber the first.
   */
  const expandedRef = useRef(expandedSet);
  expandedRef.current = expandedSet;

  /*
   * Flatten the visible subtree once per (data / accessors / expandedSet) change.
   * Iterative pre-order DFS over an explicit stack - no recursion depth limit, and
   * a collapsed node short-circuits its entire subtree. Also build id -> flat index
   * for scrollToNode. A new flat array is produced on every toggle (so the visible
   * rows re-render with their new depth/expanded state), but identity is stable
   * across scrolls, so scrolling never re-renders a mounted row.
   */
  const { data: rows, indexByKey } = useMemo(() => {
    const flat: TreeFlatRow<ItemT>[] = [];
    const byKey = new Map<string, number>();

    interface Frame {
      item: ItemT;
      depth: number;
    }
    const stack: Frame[] = [];
    for (let index = data.length - 1; index >= 0; index--) {
      stack.push({ item: data[index] as ItemT, depth: 0 });
    }

    while (stack.length > 0) {
      const { item, depth } = stack.pop() as Frame;
      const id = keyExtractor(item);
      const children = getChildren(item);
      const hasChildren = !!children && children.length > 0;
      const isExpanded = hasChildren && expandedSet.has(id);

      byKey.set(id, flat.length);
      flat.push({ id, item, depth, hasChildren, isExpanded });

      if (isExpanded && children) {
        for (let index = children.length - 1; index >= 0; index--) {
          stack.push({ item: children[index] as ItemT, depth: depth + 1 });
        }
      }
    }

    return { data: flat, indexByKey: byKey };
  }, [data, getChildren, keyExtractor, expandedSet]);

  /*
   * Flip one node's expanded state, routing through the controlled callback or the
   * internal state depending on mode. Stable identity so renderElement stays memo-
   * stable across scrolls.
   */
  const toggleId = useCallback(
    (id: string) => {
      const next = new Set(expandedRef.current);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      // Compose immediately so a second toggle in the same tick builds on this one.
      expandedRef.current = next;
      if (!isControlled) setInternalExpanded(next);
      onExpandedChange?.(next);
    },
    [isControlled, onExpandedChange]
  );

  /*
   * The Shadowlist imperative handle plus scrollToNode (id -> current flat index).
   */
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
   * Dispatch a flattened row to renderNode, enriching it with the tree affordances
   * (depth/indent/expanded/hasChildren) and a per-node toggle. Passed as
   * Shadowlist's renderElement callback; Shadowlist wraps each row in its own
   * memoized element host, so an unchanged row never re-renders while scrolling.
   */
  const renderElement = useCallback(
    ({ element, index }: { element: TreeFlatRow<ItemT>; index: number }) =>
      renderNode({
        item: element.item,
        index,
        depth: element.depth,
        isExpanded: element.isExpanded,
        hasChildren: element.hasChildren,
        indent: element.depth * indentWidth,
        toggle: () => toggleId(element.id),
      }),
    [renderNode, indentWidth, toggleId]
  );

  return (
    <Shadowlist
      ref={innerRef}
      data={rows}
      renderElement={renderElement}
      style={style}
      elementStyle={elementStyle}
      initialElementsSize={initialElementsSize}
      containerOffsetIndex={containerOffsetIndex}
      onScroll={onScroll}
      onStartReached={onStartReached}
      onEndReached={onEndReached}
      onStartReachedThreshold={onStartReachedThreshold}
      onEndReachedThreshold={onEndReachedThreshold}
      ItemSeparatorComponent={ItemSeparatorComponent}
      ListHeaderComponent={ListHeaderComponent}
      ListFooterComponent={ListFooterComponent}
      ListEmptyComponent={ListEmptyComponent}
    />
  );
}

/*
 * forwardRef + generics: the cast preserves the generic node type for callers while
 * forwarding the (extended) imperative handle.
 */
const TreeList = forwardRef(TreeListInner) as <ItemT>(
  props: TreeListProps<ItemT> & { ref?: Ref<TreeListCommands> }
) => ReactElement;

export default TreeList;
