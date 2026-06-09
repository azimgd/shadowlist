import type { Ref, ReactElement } from 'react';
import {
  useMemo,
  useState,
  useRef,
  useCallback,
  useImperativeHandle,
  forwardRef,
} from 'react';
import Shadowlist from './Shadowlist';
import { flattenTree, type TreeFlatRow } from './flattenTree';
import type {
  ShadowlistCommands,
  TreeListProps,
  TreeListCommands,
} from './types';

/*
 * TreeList is a data layer over Shadowlist, the tree analogue of SectionList: it
 * renders the visible subtree flattened by flattenTree.
 */

/* Normalise array|Set expansion inputs to a Set for O(1) membership tests. */
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
  }: TreeListProps<ItemT>,
  ref: Ref<TreeListCommands>
) {
  const innerRef = useRef<ShadowlistCommands>(null);

  /* Controlled when expandedIds is provided; else the list owns the set, seeded from initialExpandedIds. */
  const isControlled = expandedIds !== undefined;
  const [internalExpanded, setInternalExpanded] = useState<Set<string>>(() =>
    toSet(initialExpandedIds)
  );
  const expandedSet = useMemo(
    () => (isControlled ? toSet(expandedIds) : internalExpanded),
    [isControlled, expandedIds, internalExpanded]
  );

  /* Latest expansion in a ref so two toggles in one tick both compose, not clobber. */
  const expandedRef = useRef(expandedSet);
  expandedRef.current = expandedSet;

  /*
   * Flatten the visible subtree (see flattenTree); also yields id -> flat index
   * for scrollToNode.
   */
  const { rows, indexByKey } = useMemo(
    () => flattenTree(data, getChildren, keyExtractor, expandedSet),
    [data, getChildren, keyExtractor, expandedSet]
  );

  /* Flip one node's expanded state, routing through the controlled callback or internal state. */
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

  /* Shadowlist imperative handle plus scrollToNode (id -> current flat index). */
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

  /* Dispatch a flattened row to renderNode with tree affordances and a per-node toggle. */
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
    />
  );
}

/* Cast preserves the generic node type for callers across forwardRef. */
const TreeList = forwardRef(TreeListInner) as <ItemT>(
  props: TreeListProps<ItemT> & { ref?: Ref<TreeListCommands> }
) => ReactElement;

export default TreeList;
