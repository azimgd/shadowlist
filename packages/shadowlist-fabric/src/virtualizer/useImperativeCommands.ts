import { useImperativeHandle, type ComponentRef, type Ref } from 'react';
import { ShadowListView, Commands } from 'shadowlist';
import type { ShadowListCommands } from '../types';

interface ShadowListViewRef {
  current: ComponentRef<typeof ShadowListView> | null;
}

interface ElementSizesRef {
  current: Map<string, number> | null;
}

// Handed back for a list that does not track sizes, so every caller reads the same shape.
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
      // Outside [0, 1] would place the row off screen, which no caller means by it.
      const position = Math.min(1, Math.max(0, viewPosition));
      /*
       * Mount the window around the target first: the core scrolls there on the commit that
       * carries the command, and at a non-zero viewPosition the rows that fill the viewport
       * are the ones BEFORE it, which a resting window never covers.
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
