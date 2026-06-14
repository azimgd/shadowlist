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
 * Data layer over Shadowlist: flattens the visible subtree (nodes whose
 * ancestors are all expanded) into one element stream for the virtualizer.
 * Collapsed subtrees are not descended into.
 */

interface TreeFlatRow<ItemT> {
  // Stable key for the flattened row; equals keyExtractor(item).
  id: string;
  item: ItemT;
  depth: number;
  hasChildren: boolean;
  isExpanded: boolean;
}

// Normalise the array|Set expansion input to a Set for O(1) membership tests.
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
    renderElement,
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

  // Controlled when expandedIds is provided; otherwise seeded from initialExpandedIds.
  const isControlled = expandedIds !== undefined;
  const [internalExpanded, setInternalExpanded] = useState<Set<string>>(() =>
    toSet(initialExpandedIds)
  );
  const expandedSet = useMemo(
    () => (isControlled ? toSet(expandedIds) : internalExpanded),
    [isControlled, expandedIds, internalExpanded]
  );

  // Current expansion in a ref so two toggles in one tick compose instead of clobbering.
  const expandedRef = useRef(expandedSet);
  expandedRef.current = expandedSet;

  /*
   * Flatten the visible subtree via iterative pre-order DFS, building id -> flat
   * index for scrollToNode. A collapsed node short-circuits its subtree.
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

  // Flip one node's expanded state, routing through the controlled or internal path.
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

  // The Shadowlist imperative handle plus scrollToNode (id -> current flat index).
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
   * Dispatch a flattened row to renderElement, enriching it with tree affordances
   * (depth/indent/expanded/hasChildren) and a per-node toggle.
   */
  const renderRow = useCallback(
    ({ element, index }: { element: TreeFlatRow<ItemT>; index: number }) =>
      renderElement({
        element: element.item,
        index,
        depth: element.depth,
        isExpanded: element.isExpanded,
        hasChildren: element.hasChildren,
        indent: element.depth * indentWidth,
        toggle: () => toggleId(element.id),
      }),
    [renderElement, indentWidth, toggleId]
  );

  return (
    <Shadowlist
      ref={innerRef}
      data={rows}
      renderElement={renderRow}
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

// Cast preserves the generic node type for callers.
const TreeList = forwardRef(TreeListInner) as <ItemT>(
  props: TreeListProps<ItemT> & { ref?: Ref<TreeListCommands> }
) => ReactElement;

export default TreeList;
