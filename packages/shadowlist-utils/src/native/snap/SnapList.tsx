import { forwardRef } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import type { SnapItem } from 'shadowlist-utils';
import { SnapCard } from './SnapCard';

export type SnapListProps = Omit<ShadowListProps<SnapItem>, 'renderElement'> & {
  renderElement?: ShadowListProps<SnapItem>['renderElement'];
};

const renderSnapCard: ShadowListProps<SnapItem>['renderElement'] = ({
  element,
}) => <SnapCard element={element} />;

export const SnapList = forwardRef<ShadowListCommands, SnapListProps>(
  ({ renderElement, ...props }, ref) => (
    <ShadowList
      ref={ref}
      snapToItem
      renderElement={renderElement ?? renderSnapCard}
      {...props}
    />
  )
);
