import { forwardRef } from 'react';
import {
  DraggableList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import type { ContactItem } from 'shadowlist-utils';
import { ReorderRow } from './ReorderRow';

export type ReorderListProps = Omit<
  ShadowListProps<ContactItem>,
  'renderElement'
> & {
  renderElement?: ShadowListProps<ContactItem>['renderElement'];
};

const renderReorderRow: ShadowListProps<ContactItem>['renderElement'] = ({
  element,
}) => <ReorderRow element={element} />;

/*
 * A drag-to-reorder list built on `DraggableList`. Long-press a row, then drag.
 * Provide `onReorder` and persist its `data` back into your state, otherwise the
 * list snaps back.
 */
export const ReorderList = forwardRef<ShadowListCommands, ReorderListProps>(
  ({ renderElement, ...props }, ref) => (
    <DraggableList
      ref={ref}
      renderElement={renderElement ?? renderReorderRow}
      {...props}
    />
  )
);
