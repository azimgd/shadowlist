import { useCallback } from 'react';
import type { ContactItem } from 'shadowlist-utils/native';
import { useDeleteContact } from '@example/queries/contacts';
import { useNavigation } from '../navigation/Navigation';

export function useContactActions() {
  const { push } = useNavigation();
  const { mutate: deleteContact } = useDeleteContact();

  const openContact = useCallback(
    (contact: ContactItem) => push({ name: 'ContactDetail', contact }),
    [push]
  );
  const removeContact = useCallback(
    (id: string) => deleteContact(id),
    [deleteContact]
  );
  return { openContact, removeContact };
}
