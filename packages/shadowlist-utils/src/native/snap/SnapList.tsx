import { forwardRef, useCallback } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { useLabels } from '../labels';
import { defaultSnapLabels, type SnapLabels } from './labels';
import { SnapCard } from './SnapCard';
import type { SnapItem } from './types';

type RenderSnapItem = NonNullable<ShadowListProps<SnapItem>['renderItem']>;

export type SnapListProps = Omit<ShadowListProps<SnapItem>, 'renderItem'> & {
  renderItem?: RenderSnapItem;
  onPressItem?: (item: SnapItem) => void;
  labels?: Partial<SnapLabels>;
};

export const SnapList = forwardRef<ShadowListCommands, SnapListProps>(
  ({ renderItem, onPressItem, labels, ...props }, ref) => {
    const cardLabels = useLabels(defaultSnapLabels, labels);
    const renderCard = useCallback<RenderSnapItem>(
      ({ item }) => (
        <SnapCard item={item} onPress={onPressItem} labels={cardLabels} />
      ),
      [onPressItem, cardLabels]
    );
    return (
      <ShadowList
        ref={ref}
        snapToItem
        renderItem={renderItem ?? renderCard}
        {...props}
      />
    );
  }
);
