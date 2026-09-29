import { forwardRef, useCallback } from 'react';
import {
  DraggableList,
  type ShadowListCommands,
  type ShadowListProps,
} from 'shadowlist';
import type { ContactItem } from '../contacts/types';
import { useLabels } from '../labels';
import { defaultReorderLabels, type ReorderLabels } from './labels';
import { ReorderRow } from './ReorderRow';
import { useMoveItem } from './useMoveItem';

export type ReorderListProps = Omit<
  ShadowListProps<ContactItem>,
  'renderElement'
> & {
  renderElement?: ShadowListProps<ContactItem>['renderElement'];
  labels?: Partial<ReorderLabels>;
};

export const ReorderList = forwardRef<ShadowListCommands, ReorderListProps>(
  ({ renderElement, labels, ...props }, ref) => {
    const rowLabels = useLabels(defaultReorderLabels, labels);

    const moveRow = useMoveItem(props.data, props.onReorder);

    const canReorder = props.onReorder !== undefined;
    const renderReorderRow = useCallback(
      ({ element }: { element: ContactItem }) => (
        <ReorderRow
          item={element}
          onMove={canReorder ? moveRow : undefined}
          labels={rowLabels}
        />
      ),
      [canReorder, moveRow, rowLabels]
    );

    return (
      <DraggableList
        ref={ref}
        renderElement={renderElement ?? renderReorderRow}
        {...props}
      />
    );
  }
);
