import type { ContactItem } from './types';

export function getContactAccessibilityLabel(contact: ContactItem): string {
  return contact.subtitle
    ? `${contact.name}, ${contact.subtitle}`
    : contact.name;
}
