import {
  isValidElement,
  useMemo,
  useState,
  useRef,
  useCallback,
  useImperativeHandle,
  forwardRef,
  type ComponentType,
  type Ref,
  type ReactElement,
} from 'react';
import ShadowList from './ShadowList';
import type {
  ItemSeparatorProps,
  RenderItemInfo,
  ShadowListCommands,
  TreeListProps,
  TreeListCommands,
} from './types';
import { forwardedCommands } from './virtualizer';

/*
 * TreeList sits on top of ShadowList, like SectionList does for sections. It flattens the
 * nodes whose parents are all expanded into one list and skips collapsed branches. Rows
 * are keyed by node key so they survive an expand or collapse.
 */

interface TreeFlatRow<ItemT> {
  id: string;
  item: ItemT;
  depth: number;
  hasChildren: boolean;
  isExpanded: boolean;
}

/*
 * Whether the row built for this position matches the mounted one. Everything renderRow
 * reads has to match, or a reused row would show stale content.
 */
function sameRow<ItemT>(
  previous: TreeFlatRow<ItemT> | undefined,
  next: TreeFlatRow<ItemT>
): previous is TreeFlatRow<ItemT> {
  return (
    previous !== undefined &&
    previous.item === next.item &&
    previous.depth === next.depth &&
    previous.hasChildren === next.hasChildren &&
    previous.isExpanded === next.isExpanded
  );
}

function toSet(
  keys: ReadonlyArray<string> | ReadonlySet<string> | undefined
): Set<string> {
  if (!keys) return new Set();
  return keys instanceof Set
    ? new Set(keys)
    : new Set(keys as Iterable<string>);
}

function TreeListInner<ItemT>(
  {
    data,
    getChildren,
    keyExtractor,
    renderItem,
    expandedKeys,
    initialExpandedKeys,
    onExpandedChange,
    indentWidth = 16,
    getItemSizeSpec,
    ItemSeparatorComponent,
    ...rest
  }: TreeListProps<ItemT>,
  ref: Ref<TreeListCommands>
) {
  const innerRef = useRef<ShadowListCommands>(null);

  const isControlled = expandedKeys !== undefined;
  const [internalExpanded, setInternalExpanded] = useState<Set<string>>(() =>
    toSet(initialExpandedKeys)
  );
  const expandedSet = useMemo(
    () => (isControlled ? toSet(expandedKeys) : internalExpanded),
    [isControlled, expandedKeys, internalExpanded]
  );

  // Mirror the expanded set in a ref so two toggles in the same tick don't overwrite each other.
  const expandedRef = useRef(expandedSet);
  expandedRef.current = expandedSet;

  /*
   * Rows from the last flatten, by id. An unchanged row keeps its previous object. The list
   * mounts rows by identity. Without this an expand or collapse re-renders every row.
   */
  const previousRowsRef = useRef<Map<string, TreeFlatRow<ItemT>>>(new Map());

  const { data: rows, indexByKey } = useMemo(() => {
    const flat: TreeFlatRow<ItemT>[] = [];
    const byKey = new Map<string, number>();
    const previousRows = previousRowsRef.current;
    const nextRows = new Map<string, TreeFlatRow<ItemT>>();

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
      const row = { id, item, depth, hasChildren, isExpanded };
      const previousRow = previousRows.get(id);
      const finalRow = sameRow(previousRow, row) ? previousRow : row;
      nextRows.set(id, finalRow);
      flat.push(finalRow);

      if (isExpanded && children) {
        for (let index = children.length - 1; index >= 0; index--) {
          stack.push({
            item: children[index] as ItemT,
            depth: depth + 1,
          });
        }
      }
    }

    previousRowsRef.current = nextRows;

    return { data: flat, indexByKey: byKey };
  }, [data, getChildren, keyExtractor, expandedSet]);

  /*
   * Read through a ref. An inline onExpandedChange then doesn't give renderRow a new identity
   * and rebuild every mounted row on each caller render.
   */
  const onExpandedChangeRef = useRef(onExpandedChange);
  onExpandedChangeRef.current = onExpandedChange;

  const toggleKey = useCallback(
    (key: string) => {
      const next = new Set(expandedRef.current);
      if (next.has(key)) next.delete(key);
      else next.add(key);
      expandedRef.current = next;
      if (!isControlled) setInternalExpanded(next);
      onExpandedChangeRef.current?.(next);
    },
    [isControlled]
  );

  useImperativeHandle(
    ref,
    () => ({
      ...forwardedCommands(innerRef),
      scrollToNode: (key: string, viewPosition?: number) => {
        const index = indexByKey.get(key);
        if (index !== undefined) {
          innerRef.current?.scrollToIndex({ index, viewPosition });
        }
      },
    }),
    [indexByKey]
  );

  /*
   * A separator component gets the nodes on both sides, not the flattened rows. A React element
   * or a function without parameters goes through as is.
   */
  const rowSeparator = useMemo(() => {
    if (
      !ItemSeparatorComponent ||
      isValidElement(ItemSeparatorComponent) ||
      (typeof ItemSeparatorComponent === 'function' &&
        ItemSeparatorComponent.length === 0)
    ) {
      return ItemSeparatorComponent;
    }
    const Separator = ItemSeparatorComponent as ComponentType<
      ItemSeparatorProps<ItemT>
    >;
    return function TreeRowSeparator({
      highlighted,
      leadingItem,
      trailingItem,
      ...separatorProps
    }: ItemSeparatorProps<TreeFlatRow<ItemT>>) {
      return (
        <Separator
          {...separatorProps}
          highlighted={highlighted}
          leadingItem={leadingItem.item}
          trailingItem={trailingItem?.item}
        />
      );
    };
  }, [ItemSeparatorComponent]);

  /*
   * Pass one row to renderItem with its depth, indent, expanded state and a toggle.
   * index is a getter because the list only re-renders a moved row if it read the index.
   * Reading it here would re-render every row below an expand.
   */
  const renderRow = useCallback(
    (info: RenderItemInfo<TreeFlatRow<ItemT>>) => {
      const row = info.item;
      return renderItem({
        item: row.item,
        get index() {
          return info.index;
        },
        depth: row.depth,
        isExpanded: row.isExpanded,
        hasChildren: row.hasChildren,
        indent: row.depth * indentWidth,
        toggle: () => toggleKey(row.id),
        separators: info.separators,
      });
    },
    [renderItem, indentWidth, toggleKey]
  );

  const getRowSizeSpec = useMemo(
    () =>
      getItemSizeSpec
        ? (row: TreeFlatRow<ItemT>, index: number) =>
            getItemSizeSpec(row.item, index, row.depth)
        : undefined,
    [getItemSizeSpec]
  );

  return (
    <ShadowList
      {...rest}
      ref={innerRef}
      data={rows}
      ItemSeparatorComponent={rowSeparator as never}
      renderItem={renderRow}
      getItemSizeSpec={getRowSizeSpec}
    />
  );
}

const TreeList = forwardRef(TreeListInner) as <ItemT>(
  props: TreeListProps<ItemT> & { ref?: Ref<TreeListCommands> }
) => ReactElement;

export default TreeList;
