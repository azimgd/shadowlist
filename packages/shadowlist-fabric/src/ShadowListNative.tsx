import {
  createContext,
  forwardRef,
  useCallback,
  useContext,
  useEffect,
  useImperativeHandle,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
  type ComponentRef,
  type Context,
  type ReactElement,
  type Ref,
} from 'react';
import {
  Image,
  Platform,
  StyleSheet,
  Text,
  View,
  type GestureResponderEvent,
  type ImageProps,
  type TextProps,
  type ViewProps,
  type ViewStyle,
} from 'react-native';
import ShadowListView, {
  Commands,
  type OnVisibleIndicesChange,
} from './ShadowListViewNativeComponent';
import ShadowListElementView from './ShadowListElementViewNativeComponent';
import ShadowListTemplateView from './ShadowListTemplateViewNativeComponent';
import {
  createShadowListNativeId,
  encodeElementMarker,
  encodeTemplateMarker,
  toNativeStyle,
  useShadowListNativeBinding,
  type ShadowListNativeBinding,
  type ShadowListNativeHandle,
} from './native/binding';
import { SNAP_ALIGNMENT, slTrace, slTraceEnabled } from './virtualizer/helpers';
import { describeRows, planDataSync, type SyncedRows } from './native/dataSync';
import { ExtraWindow, type ExtraItem } from './native/extras';
import type {
  ShadowListNativeIndexedData,
  ShadowListNativeCommands,
  ShadowListNativeElementProps,
  ShadowListNativeProps,
  ShadowListNativeViewProps,
} from './types';

const EMPTY_STRINGS: string[] = [];
const EMPTY_NUMBERS: number[] = [];
const EMPTY_ITEMS: ReadonlyArray<never> = [];
const EMPTY_EXTRAS: ReadonlyArray<never> = [];
// iOS reports when the spinner settles. This covers a refresh that never showed one.
const REFRESH_SETTLE_FALLBACK_MS = 1200;
const LONG_PRESS_DELAY_MS = 500;
/*
 * Most in place edits a new data array may carry before it's cheaper to replace the store.
 * Each edit is its own store call. See native/dataSync.ts.
 */
const MAX_SYNC_UPDATES = 8;

interface ShadowListNativeContextValue {
  press: (
    action: string,
    elementId: string | undefined,
    target: number,
    pageX: number,
    pageY: number,
    long: boolean
  ) => void;
  // How long a touch must rest to long press, or -1 with no onElementLongPress.
  longPressDelay: () => number;
}

/*
 * Kept on the global so it survives a Fast Refresh. Otherwise the reloaded module makes a
 * second context, remounted templates read that one while the list still provides the old
 * one, and presses silently stop working.
 */
const contextGlobal = globalThis as {
  __shadowListNativeContext?: Context<ShadowListNativeContextValue | null>;
};
const ShadowListNativeContext = (contextGlobal.__shadowListNativeContext ??=
  createContext<ShadowListNativeContextValue | null>(null));

function defaultKeyExtractor(item: unknown, index: number): string {
  const id = (item as { id?: unknown } | null)?.id;
  return id === undefined || id === null ? String(index) : String(id);
}

function renderComponent(
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
}

/*
 * Keys and template names for a batch of items, as two arrays the binding takes.
 */
function describeItems<ItemT>(
  items: ReadonlyArray<ItemT>,
  keyExtractor: (item: ItemT, index: number) => string,
  templateOf: ((item: ItemT, index: number) => string) | null
): { keys: string[]; templates: string[] | null } {
  const keys = new Array<string>(items.length);
  const templates = templateOf ? new Array<string>(items.length) : null;
  for (let index = 0; index < items.length; index++) {
    const item = items[index]!;
    keys[index] = keyExtractor(item, index);
    if (templates && templateOf) templates[index] = templateOf(item, index);
  }
  return { keys, templates };
}

interface ListSession<ItemT> {
  id: string;
  handle: ShadowListNativeHandle | null;
  committed: boolean;
  // The array or indexed data last written to the store, and the row count after it.
  synced: ReadonlyArray<ItemT> | ShadowListNativeIndexedData<ItemT> | null;
  syncedCount: number;
  // Keys and templates of the synced array, so the next array can be diffed against it.
  rows: SyncedRows<ItemT> | null;
  /*
   * A command changed the rows since the last array. The store no longer matches rows, so
   * the next array replaces it like before.
   */
  diverged: boolean;
}

/*
 * A usable row count. NaN, infinity and negatives mean no rows.
 */
function indexedCount(count: number): number {
  return Number.isFinite(count) ? Math.max(0, Math.floor(count)) : 0;
}

function seedIndexed<ItemT>(
  binding: ShadowListNativeBinding,
  session: ListSession<ItemT>,
  indexed: ShadowListNativeIndexedData<ItemT>,
  templateOf: ((item: ItemT, index: number) => string) | null,
  extraWindow: ExtraWindow,
  getExtra: ((index: number) => Partial<ItemT> | null | undefined) | null,
  scrollToStart = false
): number {
  const count = indexedCount(indexed.count);
  // Static extras everywhere, plus getExtra fields near the screen. See native/extras.ts.
  const extras = extraWindow.reset(
    count,
    (indexed.extras ?? EMPTY_EXTRAS) as ReadonlyArray<{
      index: number;
      item: ExtraItem;
    }>,
    getExtra as ((index: number) => ExtraItem | null | undefined) | null
  );
  const indices = new Array<number>(extras.length);
  const items = new Array<unknown>(extras.length);
  const templates = templateOf ? new Array<string>(extras.length) : null;
  for (let entry = 0; entry < extras.length; entry++) {
    const { index, item } = extras[entry]!;
    indices[entry] = index;
    items[entry] = item;
    if (templates && templateOf)
      templates[entry] = templateOf(item as ItemT, index);
  }
  session.synced = indexed;
  session.rows = null;
  session.syncedCount = binding.setIndexed(
    session.id,
    count,
    indexed.order,
    indexed.indexField ?? 'index',
    indexed.valueField ?? 'value',
    indices,
    items,
    templates,
    scrollToStart,
    indexed.ids
  );
  return session.syncedCount;
}

function seedStore<ItemT>(
  binding: ShadowListNativeBinding,
  session: ListSession<ItemT>,
  rows: SyncedRows<ItemT>
): number {
  session.synced = rows.items;
  session.rows = rows;
  session.diverged = false;
  session.syncedCount = binding.setData(
    session.id,
    rows.items,
    rows.keys,
    rows.templates
  );
  return session.syncedCount;
}

/*
 * Write a new controlled array into the store. When it differs from the synced one by a few
 * edits, one inserted block or some removed rows, only that change is sent. Otherwise, or if
 * a store call doesn't land as planned, the whole store is replaced like before.
 */
function syncStore<ItemT>(
  binding: ShadowListNativeBinding,
  session: ListSession<ItemT>,
  rows: SyncedRows<ItemT>
): number {
  const previous = session.rows;
  const plan =
    previous !== null && !session.diverged && session.synced === previous.items
      ? planDataSync(previous, rows, MAX_SYNC_UPDATES)
      : ({ kind: 'reset' } as const);
  if (slTraceEnabled()) {
    slTrace(`native data sync op=${plan.kind} n=${rows.items.length}`);
  }
  const listId = session.id;
  const expected = rows.items.length;
  let count = -1;
  switch (plan.kind) {
    case 'none':
      count = session.syncedCount;
      break;
    case 'update': {
      let updated = true;
      for (const index of plan.indices) {
        updated =
          binding.updateItem(
            listId,
            rows.keys[index]!,
            rows.items[index],
            rows.templates ? rows.templates[index]! : null,
            true
          ) && updated;
      }
      if (updated) count = session.syncedCount;
      break;
    }
    case 'insert': {
      const end = plan.at + plan.count;
      count = binding.insertItems(
        listId,
        plan.at,
        rows.items.slice(plan.at, end),
        rows.keys.slice(plan.at, end),
        rows.templates ? rows.templates.slice(plan.at, end) : null
      );
      break;
    }
    case 'remove':
      count = binding.removeItems(listId, plan.keys);
      break;
    case 'reset':
      break;
  }
  if (count !== expected) return seedStore(binding, session, rows);
  session.synced = rows.items;
  session.rows = rows;
  session.syncedCount = count;
  return count;
}

/*
 * A list whose rows are built natively. Each template renders once, then native copies it per
 * row and fills in the row's data in the same commit. No React render per row and no JS on
 * scroll. See SHADOWLIST_NATIVE.md.
 */
function ShadowListNativeInner<ItemT>(
  {
    data,
    initialData,
    indexed,
    getExtra,
    extraPadding = 1,
    stickyHeaderIndices,
    keyExtractor = defaultKeyExtractor,
    templates,
    templateKey,
    getTemplate,
    onElementPress,
    onElementLongPress,
    longPressDelay = LONG_PRESS_DELAY_MS,
    onVisibleRangeChange,
    style,
    elementStyle,
    testID,
    inverted = false,
    followAppends = false,
    horizontal = false,
    columns = 1,
    overscan = 1,
    initialNumToRender = 10,
    padRows = 2,
    cacheRows = 64,
    initialScrollIndex = -2,
    stickyHeader = false,
    stickyFooter = false,
    autoHideHeader = false,
    autoHideFooter = false,
    snapToItem = false,
    snapToAlignment = 'start',
    refreshing = false,
    onRefresh,
    onRefreshSettle,
    refreshColor,
    onStartReached,
    onEndReached,
    onStartReachedThreshold = 1,
    onEndReachedThreshold = 1,
    onScroll,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
  }: ShadowListNativeProps<ItemT>,
  ref: Ref<ShadowListNativeCommands<ItemT>>
) {
  const viewRef = useRef<ComponentRef<typeof ShadowListView> | null>(null);
  const binding = useShadowListNativeBinding();

  /*
   * This list's native engine, created lazily in render and closed on unmount. If React
   * throws a render away, the engine is just garbage collected.
   * committed means a list node has used the engine. From then on the store may only be
   * written in the commit phase. Before that, render fills it directly so the first frame
   * already has rows.
   */
  const sessionRef = useRef<ListSession<ItemT> | null>(null);
  if (sessionRef.current === null) {
    sessionRef.current = {
      id: createShadowListNativeId(),
      handle: null,
      committed: false,
      synced: null,
      syncedCount: 0,
      rows: null,
      diverged: false,
    };
  }
  const session = sessionRef.current;
  const listId = session.id;
  const controlled = initialData === undefined;
  const source = indexed ?? (controlled ? data : initialData) ?? EMPTY_ITEMS;

  const templateOf = useMemo<((item: ItemT, index: number) => string) | null>(
    () =>
      getTemplate ??
      (templateKey
        ? (item: ItemT) =>
            String((item as Record<string, unknown>)[templateKey] ?? '')
        : null),
    [getTemplate, templateKey]
  );

  const itemsRef = useRef({ keyExtractor, templateOf });
  itemsRef.current = { keyExtractor, templateOf };

  const [storeCount, setStoreCount] = useState<number | null>(null);

  /*
   * Indexed lists. Rows near the screen get the getExtra fields. Patches go through
   * updateItem, so a field that picks a new template switches the row to it.
   */
  const getExtraRef = useRef(getExtra ?? null);
  const bindingRef = useRef(binding);
  bindingRef.current = binding;
  const extraWindowRef = useRef<ExtraWindow | null>(null);
  if (extraWindowRef.current === null) {
    const start = initialScrollIndex >= 0 ? initialScrollIndex : 0;
    extraWindowRef.current = new ExtraWindow(
      {
        update: (index, patch) => {
          const store = bindingRef.current;
          if (!store) return;
          const synced = sessionRef.current?.synced;
          const indexedData =
            synced && !Array.isArray(synced)
              ? (synced as ShadowListNativeIndexedData<ItemT>)
              : undefined;
          // Native ignores ids whose length doesn't match the count and keys rows by position.
          const ids =
            indexedData?.ids &&
            indexedData.ids.length === indexedCount(indexedData.count)
              ? indexedData.ids
              : undefined;
          const key = String(ids ? ids[index] : index);
          const { templateOf: pick } = itemsRef.current;
          let template: string | null = null;
          if (pick) {
            const previous = store.getItem(listId, key);
            if (previous === undefined) return;
            template = pick(
              { ...(previous as object), ...patch } as ItemT,
              index
            );
          }
          store.updateItem(listId, key, patch, template, false);
        },
        padding: extraPadding,
      },
      { start, end: start + Math.max(1, initialNumToRender) - 1 }
    );
  }
  const extraWindow = extraWindowRef.current;

  if (binding && !session.committed) {
    if (session.handle === null) session.handle = binding.open(listId);
    binding.configure(listId, {
      initialRows: initialNumToRender,
      padRows,
      cacheRows,
    });
    if (session.synced !== source) {
      if (indexed) {
        getExtraRef.current = getExtra ?? null;
        seedIndexed(
          binding,
          session,
          indexed,
          templateOf,
          extraWindow,
          getExtraRef.current
        );
      } else
        seedStore(
          binding,
          session,
          describeRows(
            data ?? initialData ?? EMPTY_ITEMS,
            keyExtractor,
            templateOf
          )
        );
    }
  }

  useLayoutEffect(() => {
    if (!binding) return;
    // Reopen what StrictMode's fake unmount closed.
    if (session.handle === null) session.handle = binding.open(listId);
    session.committed = true;
    return () => {
      if (session.handle !== null) binding.close(session.handle);
      session.handle = null;
    };
  }, [binding, listId, session]);

  useLayoutEffect(() => {
    if (!binding) return;
    binding.configure(listId, {
      initialRows: initialNumToRender,
      padRows,
      cacheRows,
    });
  }, [binding, listId, initialNumToRender, padRows, cacheRows]);

  // Controlled mode. A new data array replaces the store in the commit that renders it.
  useLayoutEffect(() => {
    if (!binding || !controlled || session.synced === source) return;
    const { keyExtractor: extract, templateOf: pick } = itemsRef.current;
    if (indexed) getExtraRef.current = getExtra ?? null;
    setStoreCount(
      indexed
        ? seedIndexed(
            binding,
            session,
            indexed,
            pick,
            extraWindow,
            getExtraRef.current
          )
        : syncStore(
            binding,
            session,
            describeRows(data ?? EMPTY_ITEMS, extract, pick)
          )
    );
  }, [
    binding,
    controlled,
    source,
    session,
    indexed,
    data,
    getExtra,
    extraWindow,
  ]);

  // A new getExtra reruns it for the built rows, after the effect above.
  useLayoutEffect(() => {
    if (!binding || !indexed || session.synced !== indexed) return;
    getExtraRef.current = getExtra ?? null;
    extraWindow.setGetExtra(
      (getExtra ?? null) as
        | ((index: number) => ExtraItem | null | undefined)
        | null
    );
  }, [binding, indexed, session, getExtra, extraWindow]);

  const count = storeCount ?? session.syncedCount;

  const handlersRef = useRef({
    onElementPress,
    onElementLongPress,
    longPressDelay,
    onVisibleRangeChange,
    onRefreshSettle,
  });
  handlersRef.current = {
    onElementPress,
    onElementLongPress,
    longPressDelay,
    onVisibleRangeChange,
    onRefreshSettle,
  };

  /*
   * Call onRefreshSettle once per refresh, after refreshing turns false and the spinner is
   * gone. iOS reports when the spinner finishes hiding, with a fallback if it never does.
   * On Android the spinner floats over the rows, so the list is free once it stops.
   */
  const settlePendingRef = useRef(false);
  const handleRefreshSettle = useCallback(() => {
    if (!settlePendingRef.current) return;
    settlePendingRef.current = false;
    handlersRef.current.onRefreshSettle?.();
  }, []);
  const wasRefreshingRef = useRef(refreshing);
  useEffect(() => {
    const wasRefreshing = wasRefreshingRef.current;
    wasRefreshingRef.current = refreshing;
    if (!wasRefreshing || refreshing) return;
    settlePendingRef.current = true;
    const timer = setTimeout(
      handleRefreshSettle,
      Platform.OS === 'ios' ? REFRESH_SETTLE_FALLBACK_MS : 0
    );
    return () => clearTimeout(timer);
  }, [refreshing, handleRefreshSettle]);

  const context = useMemo<ShadowListNativeContextValue>(
    () => ({
      press: (action, elementId, target, pageX, pageY, long) => {
        const handler = long
          ? handlersRef.current.onElementLongPress
          : handlersRef.current.onElementPress;
        if (!handler || !binding) return;
        const hit = binding.resolveTag(listId, target);
        if (!hit) return;
        handler({
          key: hit.key,
          index: hit.index,
          action,
          elementId,
          item: binding.getItem(listId, hit.key) as ItemT | undefined,
          repeatIndex: hit.repeatIndex >= 0 ? hit.repeatIndex : undefined,
          pageX,
          pageY,
        });
      },
      longPressDelay: () =>
        handlersRef.current.onElementLongPress
          ? handlersRef.current.longPressDelay
          : -1,
    }),
    [binding, listId]
  );

  const handleVisibleIndicesChange = useCallback(
    (event: { nativeEvent: OnVisibleIndicesChange }) => {
      const handler = handlersRef.current.onVisibleRangeChange;
      const { visibleStartIndex, visibleEndIndex } = event.nativeEvent;
      if (visibleStartIndex < 0 || visibleEndIndex < 0) return;
      const range = {
        start: Math.min(visibleStartIndex, visibleEndIndex),
        end: Math.max(visibleStartIndex, visibleEndIndex),
      };
      // Indexed rows are built only near the screen.
      if (session.synced !== null && !Array.isArray(session.synced)) {
        extraWindow.setWindow(range);
      }
      handler?.(range);
    },
    [session, extraWindow]
  );

  useImperativeHandle(ref, (): ShadowListNativeCommands<ItemT> => {
    const insert = (index: number, items: ReadonlyArray<ItemT>) => {
      if (!binding) return 0;
      session.diverged = true;
      const { keys, templates: rowTemplates } = describeItems(
        items,
        itemsRef.current.keyExtractor,
        itemsRef.current.templateOf
      );
      const next = binding.insertItems(
        listId,
        index,
        items,
        keys,
        rowTemplates
      );
      setStoreCount(next);
      return next;
    };
    const update = (key: string, patch: unknown, replace: boolean) => {
      if (!binding) return false;
      session.diverged = true;
      const { templateOf: currentTemplateOf } = itemsRef.current;
      let template: string | null = null;
      if (currentTemplateOf) {
        const previous = binding.getItem(listId, key);
        if (previous === undefined) return false;
        const next = replace
          ? (patch as ItemT)
          : ({
              ...(previous as object),
              ...(patch as object),
            } as ItemT);
        template = currentTemplateOf(next, 0);
      }
      return binding.updateItem(listId, key, patch, template, replace);
    };
    return {
      updateItem: (key, patch) => update(key, patch, false),
      replaceItem: (key, item) => update(key, item, true),
      insertItems: insert,
      appendItems: (items) => insert(-1, items),
      prependItems: (items) => insert(0, items),
      removeItems: (keys) => {
        if (!binding) return 0;
        session.diverged = true;
        const next = binding.removeItems(listId, keys);
        setStoreCount(next);
        return next;
      },
      moveItem: (key, toIndex) => {
        if (!binding) return false;
        session.diverged = true;
        return binding.moveItem(listId, key, toIndex);
      },
      setData: (items, options) => {
        if (!binding) return 0;
        session.diverged = true;
        const { keys, templates: rowTemplates } = describeItems(
          items,
          itemsRef.current.keyExtractor,
          itemsRef.current.templateOf
        );
        const next = binding.setData(
          listId,
          items,
          keys,
          rowTemplates,
          options?.scrollTo === 'start'
        );
        setStoreCount(next);
        return next;
      },
      setTemplateStyle: (template, elementId, nextStyle) =>
        binding?.setTemplateStyle(
          listId,
          template,
          elementId,
          toNativeStyle(nextStyle)
        ),
      getItem: (key) => binding?.getItem(listId, key) as ItemT | undefined,
      getKeys: () => (binding ? binding.getKeys(listId) : []),
      getCount: () => (binding ? binding.getCount(listId) : 0),
      setStartReachedEnabled: (enabled) => {
        if (viewRef.current)
          Commands.setStartReachedEnabled(viewRef.current, enabled);
      },
      setEndReachedEnabled: (enabled) => {
        if (viewRef.current)
          Commands.setEndReachedEnabled(viewRef.current, enabled);
      },
      /*
       * Go through the store, not the view, so the scroll lands after every earlier change is
       * laid out. An append then scrollToEnd shows the new row. See SHADOWLIST_NATIVE.md.
       */
      scrollToIndex: (index, viewPosition = 0) =>
        binding?.scrollToIndex(
          listId,
          Math.max(0, index),
          Math.min(1, Math.max(0, viewPosition))
        ),
      scrollToOffset: (offset, animated = true) => {
        if (viewRef.current)
          Commands.scrollToOffset(viewRef.current, offset, animated);
      },
      scrollToEnd: () => binding?.scrollToIndex(listId, -1, 0),
      scrollToStart: () => binding?.scrollToIndex(listId, -2, 0),
      refreshExtras: (indices) => extraWindow.refresh(indices),
    };
  }, [binding, listId, session, extraWindow]);

  // Sorted with no duplicates, since the host pins them in offset order.
  const stickyIndices = useMemo(
    () =>
      stickyHeaderIndices && stickyHeaderIndices.length > 0
        ? [...new Set(stickyHeaderIndices)]
            .filter((index) => index >= 0)
            .sort((a, b) => a - b)
        : EMPTY_NUMBERS,
    [stickyHeaderIndices]
  );

  const elementBaseStyle = useMemo<ViewStyle[]>(() => {
    const dimension: ViewStyle = horizontal
      ? columns > 1
        ? { height: `${100 / columns}%` }
        : styles.elementHorizontal
      : columns > 1
        ? { width: `${100 / columns}%` }
        : styles.elementVertical;
    return elementStyle
      ? [styles.element, dimension, elementStyle]
      : [styles.element, dimension];
  }, [horizontal, columns, elementStyle]);

  const templateViews = useMemo(
    () =>
      Object.entries(templates).map(([name, element]) => (
        <ShadowListElementView
          key={name}
          index={0}
          elementKey=""
          nativeID={encodeTemplateMarker(name)}
          style={elementBaseStyle}
        >
          {element}
        </ShadowListElementView>
      )),
    [templates, elementBaseStyle]
  );

  const header = useMemo(
    () => renderComponent(ListHeaderComponent),
    [ListHeaderComponent]
  );
  const footer = useMemo(
    () => renderComponent(ListFooterComponent),
    [ListFooterComponent]
  );
  const empty = useMemo(
    () => renderComponent(ListEmptyComponent),
    [ListEmptyComponent]
  );

  /*
   * Fabric creates the list's component descriptor on first use, and that is what installs
   * the binding. Until then, mount an empty list to install it. The poll above renders the
   * data a frame later.
   */
  if (!binding) {
    return (
      <ShadowListView
        style={[styles.container, style]}
        testID={testID}
        elementsAllKeys={EMPTY_STRINGS}
        elementsAnchorIgnoreKeys={EMPTY_STRINGS}
        inverted={inverted}
        horizontal={horizontal}
        stickyHeader={false}
        stickyFooter={false}
        autoHideHeader={false}
        autoHideFooter={false}
        dragEnabled={false}
        stickyHeaderIndices={EMPTY_NUMBERS}
        columns={columns}
        overscan={overscan}
        containerOffsetIndex={-2}
        refreshEnabled={false}
        refreshing={false}
        startReachedThreshold={1}
        endReachedThreshold={1}
        viewablePercentThreshold={0}
        snapToItem={false}
        snapToAlignment={0}
      />
    );
  }

  return (
    <ShadowListView
      ref={viewRef}
      style={[
        styles.container,
        style,
        horizontal && styles.containerHorizontal,
      ]}
      testID={testID}
      nativeListId={listId}
      elementsAllKeys={EMPTY_STRINGS}
      elementsAnchorIgnoreKeys={EMPTY_STRINGS}
      inverted={inverted}
      followAppends={followAppends}
      horizontal={horizontal}
      stickyHeader={stickyHeader}
      stickyFooter={stickyFooter}
      autoHideHeader={autoHideHeader}
      autoHideFooter={autoHideFooter}
      dragEnabled={false}
      stickyHeaderIndices={stickyIndices}
      columns={columns}
      overscan={overscan}
      containerOffsetIndex={initialScrollIndex}
      refreshEnabled={!!onRefresh}
      refreshing={refreshing}
      refreshColor={refreshColor}
      startReachedThreshold={onStartReachedThreshold}
      endReachedThreshold={onEndReachedThreshold}
      viewablePercentThreshold={0}
      snapToItem={snapToItem}
      snapToAlignment={SNAP_ALIGNMENT[snapToAlignment]}
      scrollEventEnabled={onScroll != null}
      onVisibleIndicesChange={
        onVisibleRangeChange || getExtra
          ? handleVisibleIndicesChange
          : undefined
      }
      onStartReached={onStartReached}
      onEndReached={onEndReached}
      onScroll={onScroll}
      onRefresh={onRefresh}
      onRefreshSettle={onRefreshSettle ? handleRefreshSettle : undefined}
    >
      {header && (
        <ShadowListTemplateView templateType="header">
          {header}
        </ShadowListTemplateView>
      )}
      <ShadowListNativeContext.Provider value={context}>
        <ShadowListTemplateView templateType="native" style={styles.templates}>
          {templateViews}
        </ShadowListTemplateView>
      </ShadowListNativeContext.Provider>
      {count === 0 && empty && (
        <ShadowListTemplateView templateType="empty">
          {empty}
        </ShadowListTemplateView>
      )}
      {footer && (
        <ShadowListTemplateView templateType="footer">
          {footer}
        </ShadowListTemplateView>
      )}
      {stickyIndices.length > 0 && (
        // Native fills this with a copy of the current sticky row and pins it.
        <ShadowListTemplateView templateType="sectionHeader" />
      )}
    </ShadowListView>
  );
}

const returnTrue = () => true;

/*
 * Tag of the row copy that was touched. The event's target is the template, since copies
 * share its handle, but each touch carries the tag of the view it hit.
 */
function touchTarget(event: GestureResponderEvent): number {
  const touch = event.nativeEvent.changedTouches?.[0];
  return (touch?.target ?? event.nativeEvent.target) as unknown as number;
}

// A touch that moves further than this is a drag, not a press, same as Pressability.
const PRESS_SLOP = 10;

function usePressResponder(action: string | undefined, id: string | undefined) {
  const context = useContext(ShadowListNativeContext);
  const pressResponder = useMemo(() => {
    if (!action || !context) return null;
    /*
     * Where the touch began. A quick drag the scroll view never takes over, like at a scroll
     * edge or a short flick on Android, still ends in a release here and must not press.
     */
    let start: { x: number; y: number; target: number } | null = null;
    // Set while a long press is pending. A release, a move past the slop, or firing clears it.
    let longTimer: ReturnType<typeof setTimeout> | null = null;
    const cancelLong = () => {
      if (longTimer !== null) clearTimeout(longTimer);
      longTimer = null;
    };
    const handlers = {
      onStartShouldSetResponder: returnTrue,
      onResponderTerminationRequest: returnTrue,
      onResponderGrant: (event: GestureResponderEvent) => {
        const origin = {
          x: event.nativeEvent.pageX,
          y: event.nativeEvent.pageY,
          target: touchTarget(event),
        };
        start = origin;
        cancelLong();
        const delay = context.longPressDelay();
        if (delay < 0) return;
        longTimer = setTimeout(() => {
          longTimer = null;
          if (start !== origin) return;
          // Don't also press on the release that follows.
          start = null;
          context.press(action, id, origin.target, origin.x, origin.y, true);
        }, delay);
      },
      onResponderMove: (event: GestureResponderEvent) => {
        const origin = start;
        if (
          origin &&
          (Math.abs(event.nativeEvent.pageX - origin.x) > PRESS_SLOP ||
            Math.abs(event.nativeEvent.pageY - origin.y) > PRESS_SLOP)
        ) {
          cancelLong();
        }
      },
      onResponderTerminate: () => {
        start = null;
        cancelLong();
      },
      onResponderRelease: (event: GestureResponderEvent) => {
        const origin = start;
        start = null;
        cancelLong();
        const { pageX, pageY } = event.nativeEvent;
        if (
          !origin ||
          Math.abs(pageX - origin.x) > PRESS_SLOP ||
          Math.abs(pageY - origin.y) > PRESS_SLOP
        ) {
          return;
        }
        context.press(action, id, touchTarget(event), pageX, pageY, false);
      },
    };
    return { handlers, cancelLong };
  }, [action, id, context]);
  // A pending long press must not fire after the element unmounts or its responder is replaced.
  useEffect(() => pressResponder?.cancelLong, [pressResponder]);
  return pressResponder?.handlers ?? null;
}

type NativeViewProps = ViewProps & ShadowListNativeViewProps;
type NativeTextProps = TextProps & ShadowListNativeElementProps;
type NativeImageProps = Partial<ImageProps> & ShadowListNativeElementProps;

/*
 * Template elements. Plain View, Text and Image with bind to map data to props, id for
 * setTemplateStyle, and action for presses. Outside a template they render like the plain ones.
 */
const NativeView = forwardRef<ComponentRef<typeof View>, NativeViewProps>(
  (
    { id, bind, action, repeat, repeatMax, nativeID, ...props },
    forwardedRef
  ) => {
    const responder = usePressResponder(action, id);
    return (
      <View
        ref={forwardedRef}
        {...props}
        {...responder}
        nativeID={
          encodeElementMarker({ id, bind, action, repeat, repeatMax }) ??
          nativeID
        }
      />
    );
  }
);

const NativeText = forwardRef<ComponentRef<typeof Text>, NativeTextProps>(
  ({ id, bind, action, nativeID, children, ...props }, forwardedRef) => {
    const responder = usePressResponder(action, id);
    return (
      <Text
        ref={forwardedRef}
        {...props}
        {...responder}
        nativeID={encodeElementMarker({ id, bind, action }) ?? nativeID}
      >
        {children ?? (bind?.text !== undefined ? ' ' : null)}
      </Text>
    );
  }
);

const NativeImage = forwardRef<ComponentRef<typeof Image>, NativeImageProps>(
  ({ id, bind, action, nativeID, ...props }, forwardedRef) => {
    const responder = usePressResponder(action, id);
    return (
      <Image
        ref={forwardedRef}
        {...(props as ImageProps)}
        {...responder}
        nativeID={encodeElementMarker({ id, bind, action }) ?? nativeID}
      />
    );
  }
);

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  containerHorizontal: {
    flexDirection: 'row',
  },
  templates: {
    display: 'none',
  },
  element: {
    position: 'absolute',
  },
  elementVertical: {
    width: '100%',
  },
  elementHorizontal: {
    height: '100%',
  },
});

type ShadowListNativeComponent = (<ItemT>(
  props: ShadowListNativeProps<ItemT> & {
    ref?: Ref<ShadowListNativeCommands<ItemT>>;
  }
) => ReactElement) & {
  View: typeof NativeView;
  Text: typeof NativeText;
  Image: typeof NativeImage;
};

const ShadowListNative = Object.assign(forwardRef(ShadowListNativeInner), {
  View: NativeView,
  Text: NativeText,
  Image: NativeImage,
}) as unknown as ShadowListNativeComponent;

export default ShadowListNative;
