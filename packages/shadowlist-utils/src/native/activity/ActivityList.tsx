import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist';
import type { ActivityData } from 'shadowlist-utils';
import { ActivityRow } from './ActivityRow';
import { ItemSeparator } from '../primitives/ItemSeparator';

export type ActivityListProps = Omit<
  ShadowlistProps<ActivityData>,
  'renderElement'
> & {
  renderElement?: ShadowlistProps<ActivityData>['renderElement'];
};

const renderActivityRow: ShadowlistProps<ActivityData>['renderElement'] = ({
  element,
}) => <ActivityRow element={element} />;

/*
 * Notification/activity feed with sticky header + footer, inset separators and
 * viewability tracking wired in. Pass `data`; supply a header/footer and
 * `onViewableItemsChanged` to surface live state.
 */
export const ActivityList = forwardRef<ShadowlistCommands, ActivityListProps>(
  ({ renderElement, ...props }, ref) => (
    <Shadowlist
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
