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
  ShadowListProps<MasonryItem>['renderElement']
>;

export type MasonryListProps = Omit<
  ShadowListProps<MasonryItem>,
  'renderElement'
> & {
  renderElement?: RenderMasonryItem;
  onPressItem?: (item: MasonryItem) => void;
  labels?: Partial<MasonryLabels>;
};

export const MasonryList = forwardRef<ShadowListCommands, MasonryListProps>(
  ({ renderElement, onPressItem, labels, ...props }, ref) => {
    const cardLabels = useLabels(defaultMasonryLabels, labels);
    const renderCard = useCallback<RenderMasonryItem>(
      ({ element }) => (
        <MasonryCard item={element} onPress={onPressItem} labels={cardLabels} />
      ),
      [onPressItem, cardLabels]
    );
    return (
      <ShadowList
        ref={ref}
        columns={3}
        renderElement={renderElement ?? renderCard}
        {...props}
      />
    );
  }
);
