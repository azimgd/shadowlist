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
import { ShadowListBase } from './ShadowList';
import type {
  ItemSeparatorProps,
  RenderItemInfo,
  ShadowListCommands,
  TreeListProps,
  TreeListCommands,
} from './types';
import { forwardedCommands } from './virtualizer';
import { useListStore } from './virtualizer/nativeStore';
import {
  buildTreeSource,
  createTreeSourceState,
  type TreeFlatRow,
  type TreeSourceState,
  type TreeToggle,
} from './virtualizer/treeSource';

/*
 * TreeList sits on top of ShadowList, like SectionList does for sections. It flattens the
 * nodes whose parents are all expanded into one list and skips collapsed branches. Rows
 * are keyed by node key so they survive an expand or collapse. An expand or collapse splices
 * only the toggled node's rows, here and in native, which keeps the row keys, see
 * treeSource.ts.
 */

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
   * The visible rows, rebuilt for new data and spliced for an expand or collapse. Rows keep
   * their objects while nothing they render from changed, since the list mounts rows by
   * identity.
   */
  const store = useListStore();
  const sourceStateRef = useRef<TreeSourceState<ItemT> | null>(null);
  if (sourceStateRef.current === null) {
    sourceStateRef.current = createTreeSourceState();
  }
  const sourceState = sourceStateRef.current;
  /*
   * The last toggle. The next flatten then knows which key changed. Without it, as when the
   * caller controls expandedKeys, both sets are compared.
   */
  const toggleRef = useRef<TreeToggle | null>(null);
  const source = useMemo(
    () =>
      buildTreeSource(
        sourceState,
        data,
        getChildren,
        keyExtractor,
        expandedSet,
        store,
        toggleRef.current
      ),
    [sourceState, data, getChildren, keyExtractor, expandedSet, store]
  );

  /*
   * Read through a ref. An inline onExpandedChange then doesn't give renderRow a new identity
   * and rebuild every mounted row on each caller render.
   */
  const onExpandedChangeRef = useRef(onExpandedChange);
  onExpandedChangeRef.current = onExpandedChange;

  const toggleKey = useCallback(
    (key: string) => {
      const previous = expandedRef.current;
      const next = new Set(previous);
      if (next.has(key)) next.delete(key);
      else next.add(key);
      expandedRef.current = next;
      toggleRef.current = { from: previous, to: next, key };
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
        const index = source.indexOfKey(key);
        if (index >= 0) {
          innerRef.current?.scrollToIndex({ index, viewPosition });
        }
      },
    }),
    [source]
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
        toggle: () => toggleKey(row.key),
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
    <ShadowListBase
      {...rest}
      ref={innerRef}
      source={source}
      store={store}
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
