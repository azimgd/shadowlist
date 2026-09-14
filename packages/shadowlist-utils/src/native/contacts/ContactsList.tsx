import { forwardRef, useCallback } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import type { ContactItem } from 'shadowlist-utils';
import { ContactRow } from './ContactRow';

export type ContactsListProps = Omit<
  ShadowListProps<ContactItem>,
  'renderElement'
> & {
  renderElement?: ShadowListProps<ContactItem>['renderElement'];
  // Forwarded to each row's swipe-to-delete button;
  onDelete?: (key: string) => void;
};

/*
 * A contacts list with avatar/name/phone rows and swipe-to-delete. Pass `data`;
 * provide `onDelete` to handle removals (or override `renderElement`).
 */
export const ContactsList = forwardRef<ShadowListCommands, ContactsListProps>(
  ({ renderElement, onDelete, ...props }, ref) => {
    /*
     * Stable unless `onDelete` changes, so ElementRenderer's per-row memoization (keyed
     * on renderElement identity) isn't defeated by every re-render of this wrapper.
     */
    const defaultRenderElement = useCallback<
      NonNullable<ShadowListProps<ContactItem>['renderElement']>
    >(
      ({ element }) => <ContactRow element={element} onDelete={onDelete} />,
      [onDelete]
    );

    return (
      <ShadowList
        ref={ref}
        renderElement={renderElement ?? defaultRenderElement}
        {...props}
      />
    );
  }
);
