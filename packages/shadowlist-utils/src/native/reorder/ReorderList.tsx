import { forwardRef, useCallback, useLayoutEffect, useRef } from 'react';
import {
  DraggableList,
  type ShadowListCommands,
  type ShadowListProps,
} from 'shadowlist';
import type { ContactItem } from '../contacts/types';
import { useLabels } from '../labels';
import { defaultReorderLabels, type ReorderLabels } from './labels';
import { ReorderRow } from './ReorderRow';

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

    // Read at action time, so the row renderer does not change identity with every reorder.
    const latest = useRef({ data: props.data, onReorder: props.onReorder });
    useLayoutEffect(() => {
      latest.current = { data: props.data, onReorder: props.onReorder };
    });

    const moveRow = useCallback((id: string, offset: -1 | 1) => {
      const { data, onReorder } = latest.current;
      const from = data.findIndex((contact) => contact.id === id);
      const to = from + offset;
      if (
        onReorder === undefined ||
        from === -1 ||
        to < 0 ||
        to >= data.length
      ) {
        return;
      }
      const next = [...data];
      next.splice(to, 0, ...next.splice(from, 1));
      onReorder({ from, to, data: next });
    }, []);

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
