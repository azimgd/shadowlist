import { useImperativeHandle, type ComponentRef, type Ref } from 'react';
import { ShadowListView, Commands } from 'shadowlist';
import type { ShadowListCommands } from '../types';

interface ShadowListViewRef {
  current: ComponentRef<typeof ShadowListView> | null;
}

interface ElementSizesRef {
  current: Map<string, number> | null;
}

// Returned when a list doesn't track sizes, so callers always get a map.
const EMPTY_SIZES: ReadonlyMap<string, number> = new Map();

export function useImperativeCommands(
  ref: Ref<ShadowListCommands>,
  viewRef: ShadowListViewRef,
  elementSizesRef: ElementSizesRef,
  seedAroundIndex: (index: number, viewPosition: number) => void
): void {
  useImperativeHandle(ref, () => ({
    setStartReachedEnabled: (enabled: boolean) => {
      if (!viewRef.current) return;
      Commands.setStartReachedEnabled(viewRef.current, enabled);
    },
    setEndReachedEnabled: (enabled: boolean) => {
      if (!viewRef.current) return;
      Commands.setEndReachedEnabled(viewRef.current, enabled);
    },
    scrollToIndex: (index: number, viewPosition: number = 0) => {
      if (!viewRef.current) return;
      // Clamp to 0 through 1, since anything else puts the row off screen.
      const position = Math.min(1, Math.max(0, viewPosition));
      /*
       * Mount the rows around the target first, since the core scrolls in the same commit.
       * With a viewPosition above 0 the rows before the target fill the screen, and a list
       * at rest has none of them mounted.
       */
      seedAroundIndex(index, position);
      Commands.scrollToIndex(viewRef.current, index, position);
    },
    scrollToOffset: (offset: number, animated: boolean = true) => {
      if (!viewRef.current) return;
      Commands.scrollToOffset(viewRef.current, offset, animated);
    },
    scrollToEnd: (animated: boolean = true) => {
      if (!viewRef.current) return;
      Commands.scrollToEnd(viewRef.current, animated);
    },
    getElementSize: (key: string) => elementSizesRef.current?.get(key),
    getElementSizes: () => elementSizesRef.current ?? EMPTY_SIZES,
  }));
}
