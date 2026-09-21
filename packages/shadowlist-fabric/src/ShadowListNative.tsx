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
import { SNAP_ALIGNMENT } from './virtualizer/helpers';
import type {
  ShadowListNativeCommands,
  ShadowListNativeElementProps,
  ShadowListNativeProps,
  ShadowListNativeViewProps,
} from './types';

const EMPTY_STRINGS: string[] = [];
const EMPTY_NUMBERS: number[] = [];
const EMPTY_ITEMS: ReadonlyArray<never> = [];
// iOS reports the spinner's settle natively; this covers a refresh that never showed one.
const REFRESH_SETTLE_FALLBACK_MS = 1200;

interface ShadowListNativeContextValue {
  press: (
    action: string,
    elementId: string | undefined,
    target: number
  ) => void;
}

/*
 * Kept on the global so it survives a Fast Refresh of this file: a re-evaluated module would
 * otherwise create a second context, and template elements remounted with the new module's code
 * would read it while the list still provides the old one (presses silently stop).
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
 * Keys and template names for a batch of items, in the parallel-array form the binding takes.
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
  // The array last written to the store with setData, and the count it left.
  synced: ReadonlyArray<ItemT> | null;
  syncedCount: number;
}

function seedStore<ItemT>(
  binding: ShadowListNativeBinding,
  session: ListSession<ItemT>,
  items: ReadonlyArray<ItemT>,
  keyExtractor: (item: ItemT, index: number) => string,
  templateOf: ((item: ItemT, index: number) => string) | null
): number {
  const { keys, templates } = describeItems(items, keyExtractor, templateOf);
  session.synced = items;
  session.syncedCount = binding.setData(session.id, items, keys, templates);
  return session.syncedCount;
}

/*
 * A list whose rows are made natively: each template is rendered once, and the native side clones
 * it per row and binds the row's data into the clone synchronously, in the commit that needs it.
 * No React render per row, no JS on scroll. See SHADOWLIST_NATIVE.md.
 */
function ShadowListNativeInner<ItemT>(
  {
    data,
    initialData,
    keyExtractor = defaultKeyExtractor,
    templates,
    templateKey,
    getTemplate,
    onElementPress,
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
   * This list's engine. `handle` holds it (a JSI host object): created lazily in render, so a
   * render React discards leaves only garbage, never a pinned engine; closed on unmount.
   * `committed`: a list node has used the engine, so the store is observable and may only be
   * written from the commit phase. Before that nobody else sees it, so render seeds it directly
   * and the node is created with its rows (no empty first frame).
   */
  const sessionRef = useRef<ListSession<ItemT> | null>(null);
  if (sessionRef.current === null) {
    sessionRef.current = {
      id: createShadowListNativeId(),
      handle: null,
      committed: false,
      synced: null,
      syncedCount: 0,
    };
  }
  const session = sessionRef.current;
  const listId = session.id;
  const controlled = initialData === undefined;
  const source = (controlled ? data : initialData) ?? EMPTY_ITEMS;

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

  if (binding && !session.committed) {
    if (session.handle === null) session.handle = binding.open(listId);
    binding.configure(listId, {
      initialRows: initialNumToRender,
      padRows,
      cacheRows,
    });
    if (session.synced !== source) {
      seedStore(binding, session, source, keyExtractor, templateOf);
    }
  }

  useLayoutEffect(() => {
    if (!binding) return;
    // A StrictMode remount re-opens what the simulated unmount closed.
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

  // Controlled: a new `data` array replaces the store, in the commit that renders it.
  useLayoutEffect(() => {
    if (!binding || !controlled || session.synced === source) return;
    const { keyExtractor: extract, templateOf: pick } = itemsRef.current;
    setStoreCount(seedStore(binding, session, source, extract, pick));
  }, [binding, controlled, source, session]);

  const count = storeCount ?? session.syncedCount;

  const handlersRef = useRef({
    onElementPress,
    onVisibleRangeChange,
    onRefreshSettle,
  });
  handlersRef.current = {
    onElementPress,
    onVisibleRangeChange,
    onRefreshSettle,
  };

  /*
   * onRefreshSettle: once per refresh, after `refreshing` turns false and the spinner is gone.
   * iOS reports the end of the retract spring (with a fallback in case it never comes); on
   * Android the spinner floats over the rows, so the list is free as soon as it stops.
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
      press: (action, elementId, target) => {
        const handler = handlersRef.current.onElementPress;
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
        });
      },
    }),
    [binding, listId]
  );

  const handleVisibleIndicesChange = useCallback(
    (event: { nativeEvent: OnVisibleIndicesChange }) => {
      const handler = handlersRef.current.onVisibleRangeChange;
      if (!handler) return;
      const { visibleStartIndex, visibleEndIndex } = event.nativeEvent;
      if (visibleStartIndex < 0 || visibleEndIndex < 0) return;
      handler({
        start: Math.min(visibleStartIndex, visibleEndIndex),
        end: Math.max(visibleStartIndex, visibleEndIndex),
      });
    },
    []
  );

  useImperativeHandle(ref, (): ShadowListNativeCommands<ItemT> => {
    const insert = (index: number, items: ReadonlyArray<ItemT>) => {
      if (!binding) return 0;
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
        const next = binding.removeItems(listId, keys);
        setStoreCount(next);
        return next;
      },
      moveItem: (key, toIndex) =>
        binding ? binding.moveItem(listId, key, toIndex) : false,
      setData: (items, options) => {
        if (!binding) return 0;
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
       * Through the store, not the view: the scroll then lands after every mutation made before
       * it is laid out (append + scrollToEnd shows the new row). See SHADOWLIST_NATIVE.md.
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
    };
  }, [binding, listId]);

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
   * Fabric builds a component descriptor the first time the component is used, and the list's
   * descriptor is what installs the binding. So until it exists, mount an empty list: that
   * installs it, and the poll above re-renders with the data a frame later.
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
      stickyHeaderIndices={EMPTY_NUMBERS}
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
        onVisibleRangeChange ? handleVisibleIndicesChange : undefined
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
    </ShadowListView>
  );
}

const returnTrue = () => true;

// A touch that travels further than this is a drag, not a press (as Pressability's slop).
const PRESS_SLOP = 10;

function usePressResponder(action: string | undefined, id: string | undefined) {
  const context = useContext(ShadowListNativeContext);
  return useMemo(() => {
    if (!action || !context) return null;
    /*
     * Where the touch began. A quick drag the scroll view never claims (e.g. at a scroll edge,
     * or a short flick on Android) still ends in a release here; it must not press.
     */
    let start: { x: number; y: number } | null = null;
    return {
      onStartShouldSetResponder: returnTrue,
      onResponderTerminationRequest: returnTrue,
      onResponderGrant: (event: GestureResponderEvent) => {
        start = { x: event.nativeEvent.pageX, y: event.nativeEvent.pageY };
      },
      onResponderTerminate: () => {
        start = null;
      },
      onResponderRelease: (event: GestureResponderEvent) => {
        const origin = start;
        start = null;
        if (
          !origin ||
          Math.abs(event.nativeEvent.pageX - origin.x) > PRESS_SLOP ||
          Math.abs(event.nativeEvent.pageY - origin.y) > PRESS_SLOP
        ) {
          return;
        }
        /*
         * The touched clone's own tag. The event's top-level target is the template element's
         * (clones share its instance handle), but each touch carries the hit view's tag.
         */
        const touch = event.nativeEvent.changedTouches?.[0];
        const target = (touch?.target ??
          event.nativeEvent.target) as unknown as number;
        context.press(action, id, target);
      },
    };
  }, [action, id, context]);
}

type NativeViewProps = ViewProps & ShadowListNativeViewProps;
type NativeTextProps = TextProps & ShadowListNativeElementProps;
type NativeImageProps = Partial<ImageProps> & ShadowListNativeElementProps;

/*
 * Template elements. Plain View/Text/Image plus `bind` (data paths -> props), `id` (for
 * setTemplateStyle) and `action` (press routing). Outside a template they render like the
 * plain component.
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
