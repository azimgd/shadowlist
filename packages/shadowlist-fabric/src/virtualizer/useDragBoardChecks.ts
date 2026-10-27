import { useEffect } from 'react';
import { Platform } from 'react-native';

/*
 * The dragListIds each mounted list registered on each drag board, with how many lists use
 * each one. Only __DEV__ builds fill it.
 */
const boards = new Map<string, Map<string, number>>();

let warnedPlatform = false;

/*
 * In __DEV__, warns about drag board props that would do nothing without a word: a board on a
 * platform that has none yet, a list on a board without a dragListId, which no row can drop
 * into, and two mounted lists with the same dragListId on one board, which the drop cannot
 * tell apart. Release builds skip it all.
 */
export function useDragBoardChecks(
  dragBoardId: string | undefined,
  dragListId: string | undefined
): void {
  useEffect(() => {
    if (!__DEV__ || !dragBoardId) {
      return undefined;
    }
    if (Platform.OS !== 'ios' && !warnedPlatform) {
      warnedPlatform = true;
      console.warn(
        `ShadowList: dragBoardId '${dragBoardId}' does nothing on ${Platform.OS}. Drag boards run on iOS ` +
          'for now. Rows still reorder within their own list.'
      );
    }
    if (!dragListId) {
      console.warn(
        `ShadowList: a list on the drag board '${dragBoardId}' has no dragListId. Rows can be dragged ` +
          'out of it but never dropped into it. Give every list on a board its own dragListId.'
      );
      return undefined;
    }
    const lists = boards.get(dragBoardId) ?? new Map<string, number>();
    boards.set(dragBoardId, lists);
    const count = (lists.get(dragListId) ?? 0) + 1;
    lists.set(dragListId, count);
    if (count > 1) {
      console.warn(
        `ShadowList: ${count} lists on the drag board '${dragBoardId}' share the dragListId '${dragListId}'. ` +
          'A drop names its list by this id. Each list needs its own. Board ids are global: two ' +
          'screens that both use one board id join one board.'
      );
    }
    return () => {
      const left = (lists.get(dragListId) ?? 1) - 1;
      if (left > 0) {
        lists.set(dragListId, left);
      } else {
        lists.delete(dragListId);
      }
      if (lists.size === 0) {
        boards.delete(dragBoardId);
      }
    };
  }, [dragBoardId, dragListId]);
}
