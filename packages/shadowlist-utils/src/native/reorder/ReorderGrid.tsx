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
  'renderItem'
> & {
  renderItem?: ShadowListProps<ReorderTileItem>['renderItem'];
  tileAspectRatio?: number;
  labels?: Partial<ReorderLabels>;
};

/*
 * Tiles in columns that you hold and drag to reorder. Mixed tile sizes stack like masonry.
 */
export const ReorderGrid = forwardRef<ShadowListCommands, ReorderGridProps>(
  (
    { renderItem, tileAspectRatio, labels, numberOfColumns = 3, ...props },
    ref
  ) => {
    const tileLabels = useLabels(defaultReorderLabels, labels);
    const moveItem = useMoveItem(props.data, props.onMoveItem);

    const canReorder = props.onMoveItem !== undefined;
    const renderTile = useCallback(
      ({ item }: { item: ReorderTileItem }) => (
        <ReorderTile
          item={item}
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
        numberOfColumns={numberOfColumns}
        renderItem={renderItem ?? renderTile}
        {...props}
      />
    );
  }
);
