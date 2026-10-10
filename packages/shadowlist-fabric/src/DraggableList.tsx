import { forwardRef, type Ref, type ReactElement } from 'react';
import ShadowList from './ShadowList';
import type { ShadowListProps, ShadowListCommands } from './types';

/*
 * A ShadowList with reorderEnabled on by default. Long press a row to pick it up and
 * drag it. onMoveItem gets the reordered data when you drop. Save it to your state,
 * or the row snaps back. Pass reorderEnabled={false} to pause dragging.
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
