import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist';
import type { SnapItem } from 'shadowlist-utils';
import { SnapCard } from './SnapCard';

export type SnapListProps = Omit<ShadowlistProps<SnapItem>, 'renderElement'> & {
  renderElement?: ShadowlistProps<SnapItem>['renderElement'];
};

const renderSnapCard: ShadowlistProps<SnapItem>['renderElement'] = ({
  element,
}) => <SnapCard element={element} />;

export const SnapList = forwardRef<ShadowlistCommands, SnapListProps>(
  ({ renderElement, ...props }, ref) => (
    <Shadowlist
      ref={ref}
      snapToItem
      renderElement={renderElement ?? renderSnapCard}
      {...props}
    />
  )
);
