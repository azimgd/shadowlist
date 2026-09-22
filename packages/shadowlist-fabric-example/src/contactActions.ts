import { useCallback } from 'react';
import { useNavigation } from '@react-navigation/native';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';
import type { ContactItem } from 'shadowlist-utils/native';
import { haptics } from './haptics';
import { useDeleteContact } from './queries/contacts';
import type { RootStackParamList } from './routes';

export function useContactActions() {
  const navigation =
    useNavigation<NativeStackNavigationProp<RootStackParamList>>();
  const { mutate: deleteContact } = useDeleteContact();

  const openContact = useCallback(
    (contact: ContactItem) => navigation.navigate('ContactDetail', { contact }),
    [navigation]
  );
  const removeContact = useCallback(
    (id: string) => {
      haptics.remove();
      deleteContact(id);
    },
    [deleteContact]
  );
  return { openContact, removeContact };
}
