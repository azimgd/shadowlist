import { forwardRef, useCallback } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { useLabels } from '../labels';
import { ItemSeparator } from '../primitives/ItemSeparator';
import { ActivityRow } from './ActivityRow';
import { defaultActivityLabels, type ActivityLabels } from './labels';
import type { ActivityItem } from './types';

type RenderActivity = NonNullable<
  ShadowListProps<ActivityItem>['renderElement']
>;

export type ActivityListProps = Omit<
  ShadowListProps<ActivityItem>,
  'renderElement'
> & {
  renderElement?: RenderActivity;
  onPressItem?: (item: ActivityItem) => void;
  formatTime?: (createdAt: Date | number) => string;
  labels?: Partial<ActivityLabels>;
};

// The separator is part of every row's content: a fresh element per render rebuilds every row.
const ITEM_SEPARATOR = <ItemSeparator />;
const VIEWABILITY_CONFIG = { itemVisiblePercentThreshold: 60 };

export const ActivityList = forwardRef<ShadowListCommands, ActivityListProps>(
  ({ renderElement, onPressItem, formatTime, labels, ...props }, ref) => {
    const mergedLabels = useLabels(defaultActivityLabels, labels);
    const renderRow = useCallback<RenderActivity>(
      ({ element }) => (
        <ActivityRow
          item={element}
          onPress={onPressItem}
          formatTime={formatTime}
          labels={mergedLabels}
        />
      ),
      [onPressItem, formatTime, mergedLabels]
    );

    return (
      <ShadowList
        ref={ref}
        stickyHeader
        stickyFooter
        ItemSeparatorComponent={ITEM_SEPARATOR}
        viewabilityConfig={VIEWABILITY_CONFIG}
        renderElement={renderElement ?? renderRow}
        {...props}
      />
    );
  }
);
