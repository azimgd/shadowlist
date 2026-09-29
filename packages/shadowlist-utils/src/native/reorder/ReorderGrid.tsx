import { forwardRef, useCallback } from 'react';
import {
  DraggableList,
  type ShadowListCommands,
  type ShadowListProps,
} from 'shadowlist';
import { useLabels } from '../labels';
import { defaultReorderLabels, type ReorderLabels } from './labels';
import { ReorderTile } from './ReorderTile';
import type { ReorderTileItem } from './types';
import { useMoveItem } from './useMoveItem';

export type ReorderGridProps = Omit<
  ShadowListProps<ReorderTileItem>,
  'renderElement'
> & {
  renderElement?: ShadowListProps<ReorderTileItem>['renderElement'];
  // Every swatch takes this ratio, for even rows. Unset keeps each item's own ratio.
  tileAspectRatio?: number;
  labels?: Partial<ReorderLabels>;
};

/*
 * Tiles in columns that you touch, hold and drag to reorder. Cells move across columns
 * while you drag, and mixed tile sizes stack like masonry.
 */
export const ReorderGrid = forwardRef<ShadowListCommands, ReorderGridProps>(
  ({ renderElement, tileAspectRatio, labels, columns = 3, ...props }, ref) => {
    const tileLabels = useLabels(defaultReorderLabels, labels);
    const moveItem = useMoveItem(props.data, props.onReorder);

    const canReorder = props.onReorder !== undefined;
    const renderTile = useCallback(
      ({ element }: { element: ReorderTileItem }) => (
        <ReorderTile
          item={element}
          onMove={canReorder ? moveItem : undefined}
          aspectRatio={tileAspectRatio}
          labels={tileLabels}
        />
      ),
      [canReorder, moveItem, tileAspectRatio, tileLabels]
    );

    return (
      <DraggableList
        ref={ref}
        columns={columns}
        renderElement={renderElement ?? renderTile}
        {...props}
      />
    );
  }
);
