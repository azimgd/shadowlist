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

interface TreeFlatRow<ItemT> {
  id: string;
  item: ItemT;
  depth: number;
  hasChildren: boolean;
  isExpanded: boolean;
}

/* The expansion prop can be an array or a Set; copy it into a Set for fast lookups. */
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
    overscan,
    keyboardAvoidingEnabled,
    keyboardAvoidingOffset,
    refreshing,
    onRefresh,
    refreshColor,
    onScroll,
    onStartReached,
    onEndReached,
    onStartReachedThreshold,
    onEndReachedThreshold,
    ItemSeparatorComponent,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
    accessible,
    accessibilityLabel,
    accessibilityRole,
    accessibilityHint,
    testID,
  }: TreeListProps<ItemT>,
  ref: Ref<TreeListCommands>
) {
  const innerRef = useRef<ShadowListCommands>(null);

  /* Controlled when expandedIds is provided; else the list owns the set, seeded from initialExpandedIds. */
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
   * Walk the tree top to bottom into a flat list of rows, skipping the children of any
   * collapsed node so off-screen subtrees cost nothing. Also records id -> flat index
   * so scrollToNode can map a node id to its row.
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

  /* Expand or collapse one node, updating internal state (or just calling the callback
   * when controlled). */
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

  /* ShadowList imperative handle plus scrollToNode (id -> current flat index). */
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

  /* Hand one flattened row to renderElement, adding tree info (depth, indent, whether it
   * has children and is expanded) and a toggle to expand/collapse that node. */
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
    <ShadowList
      ref={innerRef}
      data={rows}
      renderElement={renderRow}
      style={style}
      elementStyle={elementStyle}
      initialElementsSize={initialElementsSize}
      containerOffsetIndex={containerOffsetIndex}
      overscan={overscan}
      keyboardAvoidingEnabled={keyboardAvoidingEnabled}
      keyboardAvoidingOffset={keyboardAvoidingOffset}
      refreshing={refreshing}
      onRefresh={onRefresh}
      refreshColor={refreshColor}
      onScroll={onScroll}
      onStartReached={onStartReached}
      onEndReached={onEndReached}
      onStartReachedThreshold={onStartReachedThreshold}
      onEndReachedThreshold={onEndReachedThreshold}
      ItemSeparatorComponent={ItemSeparatorComponent}
      ListHeaderComponent={ListHeaderComponent}
      ListFooterComponent={ListFooterComponent}
      ListEmptyComponent={ListEmptyComponent}
      accessible={accessible}
      accessibilityLabel={accessibilityLabel}
      accessibilityRole={accessibilityRole}
      accessibilityHint={accessibilityHint}
      testID={testID}
    />
  );
}

/* Cast preserves the generic node type for callers across forwardRef. */
const TreeList = forwardRef(TreeListInner) as <ItemT>(
  props: TreeListProps<ItemT> & { ref?: Ref<TreeListCommands> }
) => ReactElement;

export default TreeList;
