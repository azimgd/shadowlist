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
  'renderItem'
> & {
  renderItem?: ShadowListProps<ContactItem>['renderItem'];
  labels?: Partial<ReorderLabels>;
};

export const ReorderList = forwardRef<ShadowListCommands, ReorderListProps>(
  ({ renderItem, labels, ...props }, ref) => {
    const rowLabels = useLabels(defaultReorderLabels, labels);

    const moveRow = useMoveItem(props.data, props.onMoveItem);

    const canReorder = props.onMoveItem !== undefined;
    const renderReorderRow = useCallback(
      ({ item }: { item: ContactItem }) => (
        <ReorderRow
          item={item}
          onMove={canReorder ? moveRow : undefined}
          labels={rowLabels}
        />
      ),
      [canReorder, moveRow, rowLabels]
    );

    return (
      <DraggableList
        ref={ref}
        renderItem={renderItem ?? renderReorderRow}
        {...props}
      />
    );
  }
);
