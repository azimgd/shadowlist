import { Alert } from 'react-native';
import type { ContactItem } from 'shadowlist-utils/native';
import { CHARACTER_NAMES, generateUniqueId } from './common';

export function generateContact(index: number): ContactItem {
  const characterName = CHARACTER_NAMES[index % CHARACTER_NAMES.length]!;
  const areaCode = 100 + (index % 900);
  const exchange = 200 + (index % 800);
  const lineNumber = 1000 + (index % 9000);

  return {
    id: generateUniqueId(),
    name: characterName,
    subtitle: `(${areaCode}) ${exchange}-${lineNumber}`,
  };
}

export const showContact = (contact: ContactItem) =>
  Alert.alert(contact.name, contact.subtitle);
