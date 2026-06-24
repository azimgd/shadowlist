import { forwardRef } from 'react';
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
  ({ renderElement, onDelete, ...props }, ref) => (
    <ShadowList
      ref={ref}
      renderElement={
        renderElement ??
        (({ element }) => <ContactRow element={element} onDelete={onDelete} />)
      }
      {...props}
    />
  )
);
