import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist-wasm';
import type { ContactItem } from 'shadowlist-utils';
import { ReorderRow } from './ReorderRow';

export type ReorderListProps = Omit<
  ShadowlistProps<ContactItem>,
  'renderElement'
> & {
  renderElement?: ShadowlistProps<ContactItem>['renderElement'];
};

const renderReorderRow: ShadowlistProps<ContactItem>['renderElement'] = ({
  element,
}) => <ReorderRow element={element} />;

// A drag-to-reorder list. Press and hold a row, then drag. Persist `onReorder`.
export const ReorderList = forwardRef<ShadowlistCommands, ReorderListProps>(
  ({ renderElement, ...props }, ref) => (
    <Shadowlist
      ref={ref}
      dragEnabled
      renderElement={renderElement ?? renderReorderRow}
      {...props}
    />
  )
);
