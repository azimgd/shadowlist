import type { Ref, ReactElement } from 'react';
import { forwardRef } from 'react';
import ShadowList from './ShadowList';
import type { ShadowListProps, ShadowListCommands } from './types';

/*
 * A drag-to-reorder list: ShadowList with `dragEnabled` turned on by default.
 * Long-press a row to pick it up and drag; the final move is reported through
 * `onReorder` with `data` already reordered; persist it to your state, or the
 * row snaps back on drop. Pass `dragEnabled={false}` to suspend dragging
 * without swapping the component out.
 */
function DraggableListInner<ElementT extends { id: string }>(
  { dragEnabled = true, ...props }: ShadowListProps<ElementT>,
  ref: Ref<ShadowListCommands>
) {
  return <ShadowList ref={ref} dragEnabled={dragEnabled} {...props} />;
}

const DraggableList = forwardRef(DraggableListInner) as <
  ElementT extends { id: string },
>(
  props: ShadowListProps<ElementT> & { ref?: Ref<ShadowListCommands> }
) => ReactElement;

export default DraggableList;
