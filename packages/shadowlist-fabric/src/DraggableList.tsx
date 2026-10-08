import type { Ref, ReactElement } from 'react';
import { forwardRef } from 'react';
import ShadowList from './ShadowList';
import type { ShadowListProps, ShadowListCommands } from './types';

/*
 * A ShadowList with reorderEnabled on by default. Long press a row to pick it up and
 * drag it. onReorder gets the reordered data when you drop. Save it to your state,
 * or the row snaps back. Pass reorderEnabled={false} to pause dragging.
 */
function DraggableListInner<ElementT extends { id: string }>(
  { reorderEnabled = true, ...props }: ShadowListProps<ElementT>,
  ref: Ref<ShadowListCommands>
) {
  return <ShadowList ref={ref} reorderEnabled={reorderEnabled} {...props} />;
}

const DraggableList = forwardRef(DraggableListInner) as <
  ElementT extends { id: string },
>(
  props: ShadowListProps<ElementT> & { ref?: Ref<ShadowListCommands> }
) => ReactElement;

export default DraggableList;
