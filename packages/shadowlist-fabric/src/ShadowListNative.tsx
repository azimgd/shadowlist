import {
  createContext,
  forwardRef,
  useCallback,
  useContext,
  useEffect,
  useImperativeHandle,
  useMemo,
  useRef,
  useState,
  type ComponentRef,
  type ReactElement,
  type Ref,
} from 'react';
import {
  Image,
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

interface ShadowListNativeContextValue {
  press: (
    action: string,
    elementId: string | undefined,
    target: number
  ) => void;
}

const ShadowListNativeContext =
  createContext<ShadowListNativeContextValue | null>(null);

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

/*
 * A list whose rows are made natively: each template is rendered once, and the native side clones
 * it per row and binds the row's data into the clone synchronously, in the commit that needs it.
 * No React render per row, no JS on scroll. See SHADOWLIST_NATIVE.md.
 */
function ShadowListNativeInner<ItemT>(
  {
    data,
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
  const [listId] = useState(createShadowListNativeId);
  const binding = useShadowListNativeBinding();

  const templateOf = useMemo<((item: ItemT, index: number) => string) | null>(
    () =>
      getTemplate ??
      (templateKey
        ? (item: ItemT) =>
            String((item as Record<string, unknown>)[templateKey] ?? '')
        : null),
    [getTemplate, templateKey]
  );

  /*
   * Push `data` into the native store during render, before the list node exists or commits, so
   * the first frame already has its rows. Idempotent: the store diffs by key and item, and a
   * repeat render with the same array does nothing.
   */
  const syncedRef = useRef<{
    binding: ShadowListNativeBinding;
    data: ReadonlyArray<ItemT>;
  } | null>(null);
  const [imperativeCount, setImperativeCount] = useState<number | null>(null);
  let dataCount = data.length;
  if (
    binding &&
    (syncedRef.current?.data !== data || syncedRef.current.binding !== binding)
  ) {
    binding.configure(listId, {
      initialRows: initialNumToRender,
      padRows,
      cacheRows,
    });
    const { keys, templates: rowTemplates } = describeItems(
      data,
      keyExtractor,
      templateOf
    );
    dataCount = binding.setData(listId, data, keys, rowTemplates);
    syncedRef.current = { binding, data };
    if (imperativeCount !== null) setImperativeCount(null);
  }
  const count = imperativeCount ?? dataCount;

  useEffect(() => {
    if (!binding) return;
    binding.retain(listId);
    return () => binding.release(listId);
  }, [binding, listId]);

  const handlersRef = useRef({ onElementPress, onVisibleRangeChange });
  handlersRef.current = { onElementPress, onVisibleRangeChange };

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

  const itemsRef = useRef({ keyExtractor, templateOf });
  itemsRef.current = { keyExtractor, templateOf };

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
      setImperativeCount(next);
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
        setImperativeCount(next);
        return next;
      },
      moveItem: (key, toIndex) =>
        binding ? binding.moveItem(listId, key, toIndex) : false,
      setData: (items) => {
        if (!binding) return 0;
        const { keys, templates: rowTemplates } = describeItems(
          items,
          itemsRef.current.keyExtractor,
          itemsRef.current.templateOf
        );
        const next = binding.setData(listId, items, keys, rowTemplates);
        setImperativeCount(next);
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
