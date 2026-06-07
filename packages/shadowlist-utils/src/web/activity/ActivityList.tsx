import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist-wasm';
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

// Activity feed with sticky header/footer, inset separators and viewability wired in.
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
