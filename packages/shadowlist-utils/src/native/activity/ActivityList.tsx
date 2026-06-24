import { forwardRef } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import type { ActivityData } from 'shadowlist-utils';
import { ActivityRow } from './ActivityRow';
import { ItemSeparator } from '../primitives/ItemSeparator';

export type ActivityListProps = Omit<
  ShadowListProps<ActivityData>,
  'renderElement'
> & {
  renderElement?: ShadowListProps<ActivityData>['renderElement'];
};

const renderActivityRow: ShadowListProps<ActivityData>['renderElement'] = ({
  element,
}) => <ActivityRow element={element} />;

/*
 * Notification/activity feed with sticky header + footer, inset separators and
 * viewability tracking built in. Pass `data`; supply a header/footer and
 * `onViewableItemsChanged` to surface live state.
 */
export const ActivityList = forwardRef<ShadowListCommands, ActivityListProps>(
  ({ renderElement, ...props }, ref) => (
    <ShadowList
      ref={ref}
      stickyHeader
      stickyFooter
      ItemSeparatorComponent={<ItemSeparator />}
      viewabilityConfig={{ itemVisiblePercentThreshold: 60 }}
      renderElement={renderElement ?? renderActivityRow}
      {...props}
    />
  )
);
