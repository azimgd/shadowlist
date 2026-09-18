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

type RenderSnapItem = NonNullable<ShadowListProps<SnapItem>['renderElement']>;

export type SnapListProps = Omit<ShadowListProps<SnapItem>, 'renderElement'> & {
  renderElement?: RenderSnapItem;
  onPressItem?: (item: SnapItem) => void;
  labels?: Partial<SnapLabels>;
};

export const SnapList = forwardRef<ShadowListCommands, SnapListProps>(
  ({ renderElement, onPressItem, labels, ...props }, ref) => {
    const cardLabels = useLabels(defaultSnapLabels, labels);
    const renderCard = useCallback<RenderSnapItem>(
      ({ element }) => (
        <SnapCard item={element} onPress={onPressItem} labels={cardLabels} />
      ),
      [onPressItem, cardLabels]
    );
    return (
      <ShadowList
        ref={ref}
        snapToItem
        renderElement={renderElement ?? renderCard}
        {...props}
      />
    );
  }
);
