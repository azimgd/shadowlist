import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist';
import type { ContactItem } from 'shadowlist-utils';
import { ContactRow } from './ContactRow';

export type ContactsListProps = Omit<
  ShadowlistProps<ContactItem>,
  'renderElement'
> & {
  renderElement?: ShadowlistProps<ContactItem>['renderElement'];
  // Forwarded to each row's swipe-to-delete button.
  onDelete?: (id: string) => void;
};

/*
 * A contacts list with avatar/name/phone rows and swipe-to-delete. Pass `data`;
 * provide `onDelete` to handle removals (or override `renderElement`).
 */
export const ContactsList = forwardRef<ShadowlistCommands, ContactsListProps>(
  ({ renderElement, onDelete, ...props }, ref) => (
    <Shadowlist
      ref={ref}
      renderElement={
        renderElement ??
        (({ element, index }) => (
          <ContactRow element={element} index={index} onDelete={onDelete} />
        ))
      }
      {...props}
    />
  )
);
