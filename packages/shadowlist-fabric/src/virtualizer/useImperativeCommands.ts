import { useImperativeHandle, type ComponentRef, type Ref } from 'react';
import { ShadowListView, Commands } from 'shadowlist';
import type { ShadowListCommands } from '../types';

interface ShadowListViewRef {
  current: ComponentRef<typeof ShadowListView> | null;
}

export function useImperativeCommands(
  ref: Ref<ShadowListCommands>,
  viewRef: ShadowListViewRef
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
    scrollToIndex: (index: number) => {
      if (!viewRef.current) return;
      Commands.scrollToIndex(viewRef.current, index);
    },
    scrollToOffset: (offset: number, animated: boolean = true) => {
      if (!viewRef.current) return;
      Commands.scrollToOffset(viewRef.current, offset, animated);
    },
    scrollToEnd: (animated: boolean = true) => {
      if (!viewRef.current) return;
      Commands.scrollToEnd(viewRef.current, animated);
    },
  }));
}
