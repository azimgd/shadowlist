import { useImperativeHandle, type ComponentRef, type Ref } from 'react';
import { ShadowListView, Commands } from 'shadowlist';
import { slLog } from './helpers';
import type { ShadowListCommands } from '../types';

type ShadowListViewRef = {
  current: ComponentRef<typeof ShadowListView> | null;
};

export function useImperativeCommands(
  ref: Ref<ShadowListCommands>,
  viewRef: ShadowListViewRef
): void {
  useImperativeHandle(ref, () => ({
    setStartReachedEnabled: (enabled: boolean) => {
      if (!viewRef.current) return;

      slLog('js.cmd setStartReachedEnabled', `enabled=${enabled ? 1 : 0}`);
      Commands.setStartReachedEnabled(viewRef.current, enabled);
    },
    setEndReachedEnabled: (enabled: boolean) => {
      if (!viewRef.current) return;

      slLog('js.cmd setEndReachedEnabled', `enabled=${enabled ? 1 : 0}`);
      Commands.setEndReachedEnabled(viewRef.current, enabled);
    },
    scrollToIndex: (index: number) => {
      if (!viewRef.current) return;

      slLog('js.cmd scrollToIndex', `index=${index}`);
      Commands.scrollToIndex(viewRef.current, index);
    },
    scrollToOffset: (offset: number, animated: boolean = true) => {
      if (!viewRef.current) return;

      slLog(
        'js.cmd scrollToOffset',
        `offset=${offset}`,
        `animated=${animated ? 1 : 0}`
      );
      Commands.scrollToOffset(viewRef.current, offset, animated);
    },
    scrollToEnd: (animated: boolean = true) => {
      if (!viewRef.current) return;

      slLog('js.cmd scrollToEnd', `animated=${animated ? 1 : 0}`);
      Commands.scrollToEnd(viewRef.current, animated);
    },
  }));
}
