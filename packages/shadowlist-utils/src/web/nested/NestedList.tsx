import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist-wasm';
import type { NestedItem } from 'shadowlist-utils';
import { NestedRow } from './NestedRow';

export type NestedListProps = Omit<
  ShadowlistProps<NestedItem>,
  'renderElement'
> & {
  renderElement?: ShadowlistProps<NestedItem>['renderElement'];
};

const renderNestedRow: ShadowlistProps<NestedItem>['renderElement'] = ({
  element,
}) => <NestedRow element={element} />;

// A vertical list of horizontal carousels (lists within a list).
export const NestedList = forwardRef<ShadowlistCommands, NestedListProps>(
  ({ renderElement, ...props }, ref) => (
    <Shadowlist
      ref={ref}
      renderElement={renderElement ?? renderNestedRow}
      {...props}
    />
  )
);
