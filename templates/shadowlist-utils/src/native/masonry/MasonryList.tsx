import { forwardRef, useCallback } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { useLabels } from '../labels';
import { defaultMasonryLabels, type MasonryLabels } from './labels';
import { MasonryCard } from './MasonryCard';
import type { MasonryItem } from './types';

type RenderMasonryItem = NonNullable<
  ShadowListProps<MasonryItem>['renderItem']
>;

export type MasonryListProps = Omit<
  ShadowListProps<MasonryItem>,
  'renderItem'
> & {
  renderItem?: RenderMasonryItem;
  onPressItem?: (item: MasonryItem) => void;
  labels?: Partial<MasonryLabels>;
};

export const MasonryList = forwardRef<ShadowListCommands, MasonryListProps>(
  ({ renderItem, onPressItem, labels, ...props }, ref) => {
    const cardLabels = useLabels(defaultMasonryLabels, labels);
    const renderCard = useCallback<RenderMasonryItem>(
      ({ item }) => (
        <MasonryCard item={item} onPress={onPressItem} labels={cardLabels} />
      ),
      [onPressItem, cardLabels]
    );
    return (
      <ShadowList
        ref={ref}
        numberOfColumns={3}
        renderItem={renderItem ?? renderCard}
        {...props}
      />
    );
  }
);
