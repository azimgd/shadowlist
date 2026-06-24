import { forwardRef } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import type { NestedItem } from 'shadowlist-utils';
import { NestedRow } from './NestedRow';

export type NestedListProps = Omit<
  ShadowListProps<NestedItem>,
  'renderElement'
> & {
  renderElement?: ShadowListProps<NestedItem>['renderElement'];
};

const renderNestedRow: ShadowListProps<NestedItem>['renderElement'] = ({
  element,
}) => <NestedRow element={element} />;

/*
 * A vertical list of horizontal carousels (lists-within-a-list). Each row owns
 * its own virtualized horizontal ShadowList of cards.
 */
export const NestedList = forwardRef<ShadowListCommands, NestedListProps>(
  ({ renderElement, ...props }, ref) => (
    <ShadowList
      ref={ref}
      renderElement={renderElement ?? renderNestedRow}
      {...props}
    />
  )
);
