import { forwardRef, type Ref, type ReactElement } from 'react';
import ShadowList from './ShadowList';
import type { ShadowListProps, ShadowListCommands } from './types';

/*
 * A ShadowList with reorderEnabled on by default. Long press a row to pick it up and
 * drag it. onMoveItem gets the reordered data when you drop. Save it to your state,
 * or the row snaps back. Pass reorderEnabled={false} to pause dragging. On iOS,
 * DraggableLists that share a dragBoardId form a board, where a row can be dragged into
 * another list. onMoveItemToList gets this list's data without the row. Add the row to the
 * other list's state before the item keyed destinationKey, or at its end when the key is ''.
 * destinationIndex names the same place for a key no longer in that state.
 */
function DraggableListInner<ItemT>(
  { reorderEnabled = true, ...props }: ShadowListProps<ItemT>,
  ref: Ref<ShadowListCommands>
) {
  return <ShadowList ref={ref} reorderEnabled={reorderEnabled} {...props} />;
}

const DraggableList = forwardRef(DraggableListInner) as <ItemT>(
  props: ShadowListProps<ItemT> & { ref?: Ref<ShadowListCommands> }
) => ReactElement;

export default DraggableList;
