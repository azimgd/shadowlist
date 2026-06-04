import type { ComponentRef, Ref } from 'react';
import { useRef, memo } from 'react';
import {
  useState,
  useMemo,
  useImperativeHandle,
  useCallback,
  type ReactElement,
} from 'react';
import { StyleSheet, type CodegenTypes, type ViewStyle } from 'react-native';
import {
  ShadowlistView,
  ShadowlistElementView,
  ShadowlistTemplateView,
  type OnVisibleIndicesChange,
  type OnViewableIndicesChange,
  type OnScroll,
  Commands,
} from 'shadowlist';

/*
 * A single item's viewability state, mirroring FlatList's ViewToken shape so the
 * onViewableItemsChanged contract is familiar.
 */
export interface ViewToken<ElementT> {
  item: ElementT;
  index: number;
  key: string;
  isViewable: boolean;
}

/*
 * Stable default key derivation (element.id), kept module-level so the memo/effect
 * dependencies stay referentially stable when no keyExtractor is supplied.
 */
const defaultKeyExtractor = (item: { id: string }) => item.id;

/*
 * Trace the JS <-> native state synchronization. Mirrors the SHADOWLIST_DEBUG_LOG
 * flag in the C++ core (shadowlist-core/Constants.hpp) and shares the same [SL]
 * tag, so the JS, C++ and iOS/Android logs interleave into one readable stream.
 * Only the meaningful boundaries are logged (native events applied upward,
 * imperative commands sent downward) to keep it off the per-frame scroll path.
 */
const SHADOWLIST_DEBUG_LOG = true;

const slLog = (...args: unknown[]) => {
  if (!SHADOWLIST_DEBUG_LOG) return;
  console.log('[SL]', ...args);
};

/*
 * Mounted index band [low, high] (ascending). The native core already reports a
 * buffered visible window; we mount SHADOWLIST_OVERSCAN extra rows on each side and
 * only grow/shift the band - a React re-render - when the reported window leaves
 * it. This is a low/high-water mark: while the visible window stays inside the band
 * the rows are already mounted, so we skip the state update entirely. The reported
 * window is normalised to ascending first, so inverted lists (which report
 * start > end) use exactly the same logic instead of a separate swap path.
 */
export interface VisibleBand {
  low: number;
  high: number;
}

const SHADOWLIST_OVERSCAN = 4;

export const initialBand = (
  size: number,
  initial: number,
  inverted: boolean
): VisibleBand => {
  if (size <= 0) return { low: -1, high: -1 };
  if (inverted) {
    return { low: Math.max(0, size - initial), high: size - 1 };
  }
  return { low: 0, high: Math.min(initial, size - 1) };
};

const bandToRange = (band: VisibleBand): number[] => {
  if (band.low < 0 || band.high < 0 || band.low > band.high) return [];
  const range: number[] = [];
  for (let index = band.low; index <= band.high; index++) range.push(index);
  return range;
};

interface ElementRendererProps<ElementT> {
  element: ElementT;
  index: number;
  style: ViewStyle | ViewStyle[];
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  separator: ReactElement | null;
}

const ElementRenderer = memo(function ElementRenderer<
  ElementT extends { id: string },
>({
  element,
  index,
  style,
  renderElement,
  separator,
}: ElementRendererProps<ElementT>) {
  return (
    <ShadowlistElementView index={index} style={style}>
      {renderElement({ element, index })}
      {separator}
    </ShadowlistElementView>
  );
}) as <T extends { id: string }>(
  props: ElementRendererProps<T>
) => ReactElement;

export interface ShadowlistCommands {
  setStartReachedEnabled: (enabled: boolean) => void;
  setEndReachedEnabled: (enabled: boolean) => void;
  scrollToIndex: (index: number) => void;
  scrollToOffset: (offset: number, animated?: boolean) => void;
  scrollToEnd: (animated?: boolean) => void;
}

export interface ViewabilityConfig {
  /*
   * Percent (0..100) of an item that must be visible before it counts as viewable.
   */
  itemVisiblePercentThreshold?: number;
}

export interface ShadowlistProps<ElementT extends { id: string }> {
  data: ReadonlyArray<ElementT>;
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  keyExtractor?: (item: ElementT, index: number) => string;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
  inverted?: boolean;
  horizontal?: boolean;
  stickyHeader?: boolean;
  stickyFooter?: boolean;
  columns?: number;
  containerOffsetIndex?: number;
  initialElementsSize?: number;
  ref?: Ref<ShadowlistCommands>;
  onStartReached?: () => void;
  onEndReached?: () => void;
  /*
   * Distance from the start/end, as a fraction of the visible length, at which the
   * matching callback fires (FlatList semantics). Defaults to 1.
   */
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  viewabilityConfig?: ViewabilityConfig;
  onViewableItemsChanged?: (info: {
    viewableItems: ViewToken<ElementT>[];
    changed: ViewToken<ElementT>[];
  }) => void;
  ItemSeparatorComponent?: ReactElement | (() => ReactElement | null) | null;
  ListHeaderComponent?: ReactElement | (() => ReactElement | null) | null;
  ListFooterComponent?: ReactElement | (() => ReactElement | null) | null;
  ListEmptyComponent?: ReactElement | (() => ReactElement | null) | null;
}

function Shadowlist<ElementT extends { id: string }>({
  data,
  renderElement,
  keyExtractor = defaultKeyExtractor,
  style,
  elementStyle,
  inverted = false,
  horizontal = false,
  stickyHeader = false,
  stickyFooter = false,
  columns = 1,
  containerOffsetIndex = -2,
  initialElementsSize = 20,
  ref,
  onStartReached,
  onEndReached,
  onStartReachedThreshold = 1,
  onEndReachedThreshold = 1,
  onScroll,
  viewabilityConfig,
  onViewableItemsChanged,
  ItemSeparatorComponent,
  ListHeaderComponent,
  ListFooterComponent,
  ListEmptyComponent,
}: ShadowlistProps<ElementT>) {
  const shadowlistViewRef = useRef<ComponentRef<typeof ShadowlistView> | null>(
    null
  );

  const [visibleBand, setVisibleBand] = useState<VisibleBand>(() =>
    initialBand(data.length, initialElementsSize, inverted)
  );

  const elementDimensionStyle = useMemo(() => {
    if (horizontal) {
      return columns > 1
        ? { height: `${100 / columns}%` }
        : styles.elementHorizontal;
    } else {
      return columns > 1
        ? { width: `${100 / columns}%` }
        : styles.elementVertical;
    }
  }, [horizontal, columns]);

  const elementBaseStyle = useMemo(
    () =>
      elementStyle
        ? [styles.element, elementDimensionStyle, elementStyle]
        : [styles.element, elementDimensionStyle],
    [elementDimensionStyle, elementStyle]
  );

  useImperativeHandle(ref, () => ({
    setStartReachedEnabled: (enabled: boolean) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd setStartReachedEnabled', `enabled=${enabled ? 1 : 0}`);
      Commands.setStartReachedEnabled(shadowlistViewRef.current, enabled);
    },
    setEndReachedEnabled: (enabled: boolean) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd setEndReachedEnabled', `enabled=${enabled ? 1 : 0}`);
      Commands.setEndReachedEnabled(shadowlistViewRef.current, enabled);
    },
    scrollToIndex: (index: number) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd scrollToIndex', `index=${index}`);
      Commands.scrollToIndex(shadowlistViewRef.current, index);
    },
    scrollToOffset: (offset: number, animated: boolean = true) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd scrollToOffset', `offset=${offset}`, `animated=${animated ? 1 : 0}`);
      Commands.scrollToOffset(shadowlistViewRef.current, offset, animated);
    },
    scrollToEnd: (animated: boolean = true) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd scrollToEnd', `animated=${animated ? 1 : 0}`);
      Commands.scrollToEnd(shadowlistViewRef.current, animated);
    },
  }));

  const handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  > = useCallback(
    (event) => {
      const { visibleStartIndex, visibleEndIndex } = event.nativeEvent;
      if (visibleStartIndex === -1 || visibleEndIndex === -1) return;
      // Inverted lists report start > end; normalise to an ascending window.
      const windowLow = Math.min(visibleStartIndex, visibleEndIndex);
      const windowHigh = Math.max(visibleStartIndex, visibleEndIndex);

      setVisibleBand((prevBand) => {
        // Low/high-water: the reported window is already inside the mounted band,
        // so those rows are mounted - skip the state update (and the re-render).
        if (
          prevBand.low >= 0 &&
          windowLow >= prevBand.low &&
          windowHigh <= prevBand.high
        ) {
          return prevBand;
        }
        const low = Math.max(0, windowLow - SHADOWLIST_OVERSCAN);
        const high = Math.min(data.length - 1, windowHigh + SHADOWLIST_OVERSCAN);
        slLog(
          'js.onVisibleIndicesChange apply',
          `window=[${windowLow}..${windowHigh}]`,
          `band=[${low}..${high}]`
        );
        return { low, high };
      });
    },
    [data.length]
  );

  const visibleRange = useMemo(() => bandToRange(visibleBand), [visibleBand]);

  const elementsAllKeys = useMemo(
    () => data.map((element, index) => keyExtractor(element, index)),
    [data, keyExtractor]
  );

  const prevViewableRef = useRef<ViewToken<ElementT>[]>([]);

  const handleViewableIndicesChange: CodegenTypes.DirectEventHandler<
    OnViewableIndicesChange,
    never
  > = useCallback(
    (event) => {
      if (!onViewableItemsChanged) return;

      const { viewableStartIndex, viewableEndIndex } = event.nativeEvent;
      // The core reports the higher index first for inverted lists; normalise to
      // an ascending range before mapping back to items.
      const lo = Math.min(viewableStartIndex, viewableEndIndex);
      const hi = Math.max(viewableStartIndex, viewableEndIndex);

      const viewableItems: ViewToken<ElementT>[] = [];
      if (viewableStartIndex !== -1 && viewableEndIndex !== -1) {
        for (let index = lo; index <= hi; index++) {
          const item = data[index];
          if (!item) continue;
          viewableItems.push({
            item,
            index,
            key: keyExtractor(item, index),
            isViewable: true,
          });
        }
      }

      const currentKeys = new Set(viewableItems.map((token) => token.key));
      const prevKeys = new Set(prevViewableRef.current.map((token) => token.key));

      const changed: ViewToken<ElementT>[] = [
        ...viewableItems.filter((token) => !prevKeys.has(token.key)),
        ...prevViewableRef.current
          .filter((token) => !currentKeys.has(token.key))
          .map((token) => ({ ...token, isViewable: false })),
      ];

      prevViewableRef.current = viewableItems;

      if (changed.length > 0) {
        slLog('js.onViewableItemsChange', `viewable=[${lo}..${hi}]`, `changed=${changed.length}`);
        onViewableItemsChanged({ viewableItems, changed });
      }
    },
    [data, keyExtractor, onViewableItemsChanged]
  );

  const viewablePercentThreshold =
    (viewabilityConfig?.itemVisiblePercentThreshold ?? 0) / 100;

  const renderComponent = (
    component: ReactElement | (() => ReactElement | null) | null | undefined
  ): ReactElement | null => {
    if (!component) return null;
    return typeof component === 'function' ? component() : component;
  };

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

  const separator = useMemo(
    () => renderComponent(ItemSeparatorComponent),
    [ItemSeparatorComponent]
  );

  return (
    <ShadowlistView
      ref={shadowlistViewRef}
      style={[styles.container, style]}
      onVisibleIndicesChange={handleVisibleIndicesChange}
      onViewableIndicesChange={
        onViewableItemsChanged ? handleViewableIndicesChange : undefined
      }
      elementsAllKeys={elementsAllKeys}
      inverted={inverted}
      horizontal={horizontal}
      stickyHeader={stickyHeader}
      stickyFooter={stickyFooter}
      columns={columns}
      containerOffsetIndex={containerOffsetIndex}
      startReachedThreshold={onStartReachedThreshold}
      endReachedThreshold={onEndReachedThreshold}
      viewablePercentThreshold={viewablePercentThreshold}
      onStartReached={onStartReached}
      onEndReached={onEndReached}
      onScroll={onScroll}
    >
      {header && (
        <ShadowlistTemplateView templateType="header">
          {header}
        </ShadowlistTemplateView>
      )}
      {data.length === 0 && empty ? (
        <ShadowlistTemplateView templateType="empty">
          {empty}
        </ShadowlistTemplateView>
      ) : (
        visibleRange.map((index) => {
          const element = data[index];

          if (!element) return null;

          return (
            <ElementRenderer
              key={keyExtractor(element, index)}
              element={element}
              index={index}
              style={elementBaseStyle}
              renderElement={renderElement}
              separator={index < data.length - 1 ? separator : null}
            />
          );
        })
      )}
      {footer && (
        <ShadowlistTemplateView templateType="footer">
          {footer}
        </ShadowlistTemplateView>
      )}
    </ShadowlistView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
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

export default Shadowlist;
