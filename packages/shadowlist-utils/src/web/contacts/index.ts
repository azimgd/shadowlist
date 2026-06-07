import { ContactsList } from './ContactsList';
import { ContactRow } from './ContactRow';

export type { ContactsListProps } from './ContactsList';
export type { ContactRowProps } from './ContactRow';

// Contacts domain: <Contacts.List data={people} onDelete={...} /> + <Contacts.Row />.
export const Contacts = {
  List: ContactsList,
  Row: ContactRow,
};
