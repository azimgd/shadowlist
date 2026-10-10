import { useImperativeHandle, type ComponentRef, type Ref } from 'react';
import ShadowListView, { Commands } from '../ShadowListViewNativeComponent';
import type {
  AnchorState,
  ScrollToIndexFailedInfo,
  ScrollToIndexParams,
  ScrollToItemParams,
  ShadowListCommands,
} from '../types';
import { nativeTagOf } from './helpers';

interface ShadowListViewRef {
  current: ComponentRef<typeof ShadowListView> | null;
}

interface ElementSizesRef {
  current: Map<string, number> | null;
}

/*
 * What the commands read at call time, refreshed on every render.
 */
export interface CommandSource {
  data: ReadonlyArray<unknown>;
  keyToIndex: ReadonlyMap<string, number>;
  highestMountedIndex: number;
  contentLength: number;
  onScrollToIndexFailed: ((info: ScrollToIndexFailedInfo) => void) | undefined;
  seedAroundIndex: (index: number, viewPosition: number) => void;
  recordInteraction: () => void;
  selectIndex: (index: number) => void;
  deselectIndex: (index: number) => void;
  getSelectedIndices: () => number[];
  requestAnchorState: () => Promise<AnchorState | null>;
  restoreAnchorState: (state: AnchorState) => void;
}

interface CommandSourceRef {
  current: CommandSource;
}

/*
 * Returned when a list doesn't track sizes. Callers always get a map.
 */
const EMPTY_SIZES: ReadonlyMap<string, number> = new Map();

function clampViewPosition(viewPosition: number | undefined): number {
  // Clamp to 0 through 1, since anything else puts the row off screen.
  return Math.min(1, Math.max(0, viewPosition ?? 0));
}

/*
 * The ref commands, in positional and object forms. Positional scrollToItem does not animate
 * unless asked. The object forms animate by default.
 */
export function useImperativeCommands(
  ref: Ref<ShadowListCommands>,
  viewRef: ShadowListViewRef,
  elementSizesRef: ElementSizesRef,
  sourceRef: CommandSourceRef
): void {
  useImperativeHandle(ref, () => {
    /*
     * Mount the rows around the target first, since the core scrolls in the same commit.
     * With a viewPosition above 0 the rows before the target fill the screen, and a list
     * at rest has none of them mounted.
     */
    const scrollToRow = (
      index: number,
      viewPosition: number,
      viewOffset: number,
      animated: boolean
    ) => {
      if (!viewRef.current) return;
      const position = clampViewPosition(viewPosition);
      sourceRef.current.seedAroundIndex(index, position);
      Commands.scrollToItem(
        viewRef.current,
        index,
        position,
        Number.isFinite(viewOffset) ? viewOffset : 0,
        animated
      );
    };

    const scrollToIndex = (params: ScrollToIndexParams) => {
      const source = sourceRef.current;
      const { index } = params;
      if (
        !Number.isInteger(index) ||
        index < 0 ||
        index >= source.data.length
      ) {
        const count = source.data.length;
        const info: ScrollToIndexFailedInfo = {
          index,
          highestMeasuredFrameIndex: source.highestMountedIndex,
          averageItemLength: count > 0 ? source.contentLength / count : 0,
        };
        if (source.onScrollToIndexFailed) {
          source.onScrollToIndexFailed(info);
        } else if (__DEV__) {
          console.warn(
            `scrollToIndex out of range: requested index ${index} but the list has ${count} items. ` +
              'Pass onScrollToIndexFailed to handle this.'
          );
        }
        return;
      }
      scrollToRow(
        index,
        params.viewPosition ?? 0,
        params.viewOffset ?? 0,
        params.animated ?? true
      );
    };

    const scrollToItem = (
      indexOrParams: number | ScrollToItemParams,
      viewPosition?: number,
      animated?: boolean
    ) => {
      if (typeof indexOrParams === 'number') {
        scrollToRow(indexOrParams, viewPosition ?? 0, 0, animated ?? false);
        return;
      }
      const index = sourceRef.current.data.indexOf(indexOrParams.item);
      if (index < 0) return;
      scrollToRow(
        index,
        indexOrParams.viewPosition ?? 0,
        indexOrParams.viewOffset ?? 0,
        indexOrParams.animated ?? true
      );
    };

    const scrollToOffset = (
      offsetOrParams: number | { offset: number; animated?: boolean },
      animated?: boolean
    ) => {
      if (!viewRef.current) return;
      const params =
        typeof offsetOrParams === 'number'
          ? { offset: offsetOrParams, animated: animated ?? true }
          : offsetOrParams;
      // The start is the one offset whose rows are known. Mount them before the jump.
      if (params.offset <= 0 && sourceRef.current.data.length > 0) {
        sourceRef.current.seedAroundIndex(0, 0);
      }
      Commands.scrollToOffset(
        viewRef.current,
        params.offset,
        params.animated ?? true
      );
    };

    const scrollToEnd = (
      animatedOrParams?: boolean | { animated?: boolean }
    ) => {
      if (!viewRef.current) return;
      const animated =
        typeof animatedOrParams === 'object'
          ? (animatedOrParams.animated ?? true)
          : (animatedOrParams ?? true);
      // Mount the last rows first. A host that lands at once would show them blank.
      const count = sourceRef.current.data.length;
      if (count > 0) {
        sourceRef.current.seedAroundIndex(count - 1, 1);
      }
      Commands.scrollToEnd(viewRef.current, animated);
    };

    return {
      setStartReachedEnabled: (enabled: boolean) => {
        if (!viewRef.current) return;
        Commands.setStartReachedEnabled(viewRef.current, enabled);
      },
      setEndReachedEnabled: (enabled: boolean) => {
        if (!viewRef.current) return;
        Commands.setEndReachedEnabled(viewRef.current, enabled);
      },
      scrollToItem: scrollToItem as ShadowListCommands['scrollToItem'],
      scrollToIndex,
      scrollToOffset: scrollToOffset as ShadowListCommands['scrollToOffset'],
      scrollToEnd: scrollToEnd as ShadowListCommands['scrollToEnd'],
      flashScrollIndicators: () => {
        if (!viewRef.current) return;
        Commands.flashScrollIndicators(viewRef.current);
      },
      recordInteraction: () => sourceRef.current.recordInteraction(),
      /*
       * The list's native view. It hosts the scroll view, which has no React instance of its
       * own under Fabric.
       */
      getNativeScrollRef: () => viewRef.current,
      getScrollResponder: () => viewRef.current,
      getScrollableNode: () => {
        const tag = nativeTagOf(viewRef.current);
        return tag === -1 ? null : tag;
      },
      getElementSize: (key: string) => elementSizesRef.current?.get(key),
      getElementSizes: () => elementSizesRef.current ?? EMPTY_SIZES,
      selectItem: (index: number) => sourceRef.current.selectIndex(index),
      deselectItem: (index: number) => sourceRef.current.deselectIndex(index),
      getSelectedIndices: () => sourceRef.current.getSelectedIndices(),
      closeSwipeActions: () => {
        if (!viewRef.current) return;
        Commands.closeSwipeActions(viewRef.current);
      },
      getAnchorState: () => sourceRef.current.requestAnchorState(),
      restoreAnchorState: (state: AnchorState) =>
        sourceRef.current.restoreAnchorState(state),
    };
  });
}

/*
 * Commands that call through to an inner list's ref, for lists that wrap ShadowList.
 */
export function forwardedCommands(innerRef: {
  current: ShadowListCommands | null;
}): ShadowListCommands {
  return {
    setStartReachedEnabled: (enabled) =>
      innerRef.current?.setStartReachedEnabled(enabled),
    setEndReachedEnabled: (enabled) =>
      innerRef.current?.setEndReachedEnabled(enabled),
    scrollToItem: ((...args: [number, number?, boolean?]) =>
      innerRef.current?.scrollToItem(
        ...args
      )) as ShadowListCommands['scrollToItem'],
    scrollToIndex: (params) => innerRef.current?.scrollToIndex(params),
    scrollToOffset: ((...args: [number, boolean?]) =>
      innerRef.current?.scrollToOffset(
        ...args
      )) as ShadowListCommands['scrollToOffset'],
    scrollToEnd: ((...args: [boolean?]) =>
      innerRef.current?.scrollToEnd(
        ...args
      )) as ShadowListCommands['scrollToEnd'],
    flashScrollIndicators: () => innerRef.current?.flashScrollIndicators(),
    recordInteraction: () => innerRef.current?.recordInteraction(),
    getNativeScrollRef: () => innerRef.current?.getNativeScrollRef() ?? null,
    getScrollResponder: () => innerRef.current?.getScrollResponder() ?? null,
    getScrollableNode: () => innerRef.current?.getScrollableNode() ?? null,
    getElementSize: (key) => innerRef.current?.getElementSize(key),
    getElementSizes: () => innerRef.current?.getElementSizes() ?? EMPTY_SIZES,
    selectItem: (index) => innerRef.current?.selectItem(index),
    deselectItem: (index) => innerRef.current?.deselectItem(index),
    getSelectedIndices: () => innerRef.current?.getSelectedIndices() ?? [],
    closeSwipeActions: () => innerRef.current?.closeSwipeActions(),
    getAnchorState: () =>
      innerRef.current?.getAnchorState() ?? Promise.resolve(null),
    restoreAnchorState: (state) => innerRef.current?.restoreAnchorState(state),
  };
}
