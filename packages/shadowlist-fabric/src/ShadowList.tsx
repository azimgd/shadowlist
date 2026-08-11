import {
  useRef,
  useMemo,
  useCallback,
  forwardRef,
  type ComponentRef,
  type Ref,
  type ReactElement,
} from 'react';
import { StyleSheet, type ViewStyle } from 'react-native';
import { ShadowListView, ShadowListTemplateView } from 'shadowlist';
import type { ShadowListProps, ShadowListCommands } from './types';
import { useKeyboardInset } from './keyboard';
import {
  ElementRenderer,
  SNAP_ALIGNMENT,
  useMountedRange,
  useRefreshDefer,
  useDragReorder,
  usePersistentKeys,
  useViewability,
  useImperativeCommands,
} from './virtualizer';

export { initialMountedRange, type MountedRange } from './virtualizer';

const defaultKeyExtractor = (item: { id: string }) => item.id;

const renderComponent = (
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null => {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
};

/*
 * The JS layer over the native <ShadowListView>. It narrows the full `data` prop down
 * to the rows visible in the viewport, and routes the native scroll/drag/refresh events
 * into the hooks below.
 */
function ShadowListInner<ElementT extends { id: string }>(
  {
    data: dataProp,
    renderElement,
    keyExtractor = defaultKeyExtractor,
    style,
    elementStyle,
    inverted = false,
    horizontal = false,
    stickyHeader = false,
    stickyFooter = false,
    autoHideHeader = false,
    autoHideFooter = false,
    dragEnabled = false,
    onReorder,
    columns = 1,
    overscan = 1,
    persistentKeys,
    stickyHeaderIndices,
    renderStickyHeaderOverlay,
    containerOffsetIndex = -2,
    keyboardAvoidingEnabled = false,
    keyboardAvoidingOffset = 0,
    refreshing = false,
    onRefresh,
    refreshColor,
    initialElementsSize = 20,
    onStartReached,
    onEndReached,
    onStartReachedThreshold = 1,
    onEndReachedThreshold = 1,
    onScroll,
    snapToItem = false,
    snapToAlignment = 'start',
    viewabilityConfig,
    onViewableItemsChanged,
    ItemSeparatorComponent,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
    accessible,
    accessibilityLabel,
    accessibilityRole,
    accessibilityHint,
    testID,
  }: ShadowListProps<ElementT>,
  ref: Ref<ShadowListCommands>
) {
  const shadowlistViewRef = useRef<ComponentRef<typeof ShadowListView> | null>(
    null
  );

  // How far the keyboard overlaps the list, sent to native as a bottom inset so the
  // keyboard never hides content. 0 when keyboard-avoiding is off.
  const contentInsetBottom = useKeyboardInset(shadowlistViewRef, {
    enabled: keyboardAvoidingEnabled,
    offset: keyboardAvoidingOffset,
  });

  // Pull-to-refresh just forwards to the consumer; they own the spinner via `refreshing`.
  const handleRefresh = useCallback(() => {
    onRefresh?.();
  }, [onRefresh]);

  // While a refresh runs, hold back incoming data until the spinner finishes retracting
  // so rows don't jump under the user. `data` is the list we actually render.
  const { data, handleRefreshSettle } = useRefreshDefer({
    data: dataProp,
    refreshing,
    onRefresh,
    inverted,
    horizontal,
  });

  // The virtualization window: which row indices are currently mounted. Rows outside
  // it are not rendered at all.
  const { mountedIndices, handleVisibleIndicesChange } = useMountedRange({
    data,
    keyExtractor,
    initialElementsSize,
    inverted,
    containerOffsetIndex,
  });

  // Drag-to-reorder: keeps the picked-up row mounted while dragged and reports the new
  // order on drop. draggedIndices = the mounted rows plus the picked-up one.
  const {
    renderIndices: draggedIndices,
    handleDragStart,
    handleDragEnd,
  } = useDragReorder({
    data,
    keyExtractor,
    mountedIndices,
    dragEnabled,
    onReorder,
  });

  // Persistent rows: force-mount the keys in persistentKeys at their natural position,
  // never virtualized away. renderIndices = the final mounted set we render below.
  const renderIndices = usePersistentKeys({
    data,
    keyExtractor,
    persistentKeys,
    renderIndices: draggedIndices,
  });

  // Viewability: which rows count as "viewed" (onViewableItemsChanged) and which section
  // header is currently pinned (drives the sticky overlay).
  const { activeStickyIndex, handleViewableIndicesChange } = useViewability({
    data,
    keyExtractor,
    stickyHeaderIndices,
    onViewableItemsChanged,
  });

  // Connect the public imperative handle (scrollTo*, set*ReachedEnabled) onto `ref`.
  useImperativeCommands(ref, shadowlistViewRef);

  // Each row's size along the cross axis: a 1/columns fraction for multi-column grids,
  // otherwise it fills the cross axis (full width vertically, full height horizontally).
  const elementDimensionStyle = useMemo<ViewStyle>(() => {
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

  // Every row's key, handed to native so it can track row identity across data updates.
  const elementsAllKeys = useMemo(
    () => data.map((element, index) => keyExtractor(element, index)),
    [data, keyExtractor]
  );

  // Fraction of a row (0..1) that must be on screen to count as viewable; native applies it.
  const viewablePercentThreshold =
    (viewabilityConfig?.itemVisiblePercentThreshold ?? 0) / 100;

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

  /*
   * Sticky section-header overlay: a single template view whose content is swapped to
   * the currently pinned section's header. Null while scrolled above the first header.
   */
  const stickyEnabled = Boolean(
    stickyHeaderIndices &&
    stickyHeaderIndices.length > 0 &&
    renderStickyHeaderOverlay
  );

  const stickyOverlay = useMemo(
    () =>
      stickyEnabled && activeStickyIndex >= 0 && renderStickyHeaderOverlay
        ? renderStickyHeaderOverlay(activeStickyIndex)
        : null,
    [stickyEnabled, activeStickyIndex, renderStickyHeaderOverlay]
  );

  return (
    <ShadowListView
      ref={shadowlistViewRef}
      style={[styles.container, style]}
      accessible={accessible}
      accessibilityLabel={accessibilityLabel}
      accessibilityRole={accessibilityRole}
      accessibilityHint={accessibilityHint}
      testID={testID}
      onVisibleIndicesChange={handleVisibleIndicesChange}
      onViewableIndicesChange={
        onViewableItemsChanged || stickyEnabled
          ? handleViewableIndicesChange
          : undefined
      }
      elementsAllKeys={elementsAllKeys}
      elementsAnchorIgnoreKeys={[]}
      inverted={inverted}
      horizontal={horizontal}
      stickyHeader={stickyHeader}
      stickyFooter={stickyFooter}
      autoHideHeader={autoHideHeader}
      autoHideFooter={autoHideFooter}
      stickyHeaderIndices={stickyHeaderIndices ?? []}
      columns={columns}
      overscan={overscan}
      containerOffsetIndex={containerOffsetIndex}
      contentInsetBottom={contentInsetBottom}
      refreshEnabled={!!onRefresh}
      refreshing={refreshing}
      refreshColor={refreshColor}
      startReachedThreshold={onStartReachedThreshold}
      endReachedThreshold={onEndReachedThreshold}
      viewablePercentThreshold={viewablePercentThreshold}
      snapToItem={snapToItem}
      snapToAlignment={SNAP_ALIGNMENT[snapToAlignment]}
      dragEnabled={dragEnabled}
      onStartReached={onStartReached}
      onEndReached={onEndReached}
      onScroll={onScroll}
      onRefresh={onRefresh ? handleRefresh : undefined}
      onRefreshSettle={onRefresh ? handleRefreshSettle : undefined}
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
    >
      {header && (
        <ShadowListTemplateView templateType="header">
          {header}
        </ShadowListTemplateView>
      )}
      {data.length === 0 && empty ? (
        <ShadowListTemplateView templateType="empty">
          {empty}
        </ShadowListTemplateView>
      ) : (
        renderIndices.map((index) => {
          const element = data[index];

          if (!element) return null;

          return (
            <ElementRenderer
              key={keyExtractor(element, index)}
              element={element}
              index={index}
              elementKey={keyExtractor(element, index)}
              style={elementBaseStyle}
              renderElement={renderElement}
              separator={index < data.length - 1 ? separator : null}
            />
          );
        })
      )}
      {footer && (
        <ShadowListTemplateView templateType="footer">
          {footer}
        </ShadowListTemplateView>
      )}
      {stickyEnabled && (
        <ShadowListTemplateView templateType="sectionHeader">
          {stickyOverlay}
        </ShadowListTemplateView>
      )}
    </ShadowListView>
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

const ShadowList = forwardRef(ShadowListInner) as <
  ElementT extends { id: string },
>(
  props: ShadowListProps<ElementT> & { ref?: Ref<ShadowListCommands> }
) => ReactElement;

export default ShadowList;
