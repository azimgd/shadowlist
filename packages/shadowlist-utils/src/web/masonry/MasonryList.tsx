import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist-wasm';
import type { MasonryItem } from 'shadowlist-utils';
import { MasonryCard } from './MasonryCard';

export type MasonryListProps = Omit<
  ShadowlistProps<MasonryItem>,
  'renderElement'
> & {
  renderElement?: ShadowlistProps<MasonryItem>['renderElement'];
};

const renderMasonryCard: ShadowlistProps<MasonryItem>['renderElement'] = ({
  element,
}) => <MasonryCard element={element} />;

// A multi-column grid of variable-height image cards (defaults to 3 columns).
export const MasonryList = forwardRef<ShadowlistCommands, MasonryListProps>(
  ({ renderElement, ...props }, ref) => (
    <Shadowlist
      ref={ref}
      columns={3}
      renderElement={renderElement ?? renderMasonryCard}
      {...props}
    />
  )
);
