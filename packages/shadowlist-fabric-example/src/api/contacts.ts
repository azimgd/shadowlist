import type { ContactItem } from 'shadowlist-utils/native';
import { generateContact } from '../fixtures/contacts';
import { Collection } from './Collection';
import { request, type RequestSignal } from './network';
import { listCount } from '../launchSettings';

export type InsertPosition = 'start' | 'end';

let generatedCount = 0;
const generateContacts = (count: number) =>
  Array.from({ length: count }, () => generateContact(generatedCount++));

// The address book: small enough to fetch whole, as most contact APIs do. SLCount sets its size.
const contacts = new Collection<ContactItem>({
  seed: () => generateContacts(listCount),
});

export function fetchContacts(signal?: RequestSignal): Promise<ContactItem[]> {
  return request(() => contacts.all(), signal);
}

export function createContacts({
  count,
  position,
}: {
  count: number;
  position: InsertPosition;
}): Promise<ContactItem[]> {
  return request(() => {
    const created = generateContacts(count);
    if (position === 'start') contacts.insertAtStart(created);
    else contacts.insertAtEnd(created);
    return created;
  });
}

export function deleteContact(id: string): Promise<void> {
  return request(() => contacts.remove([id]));
}

// A user-ordered favourites list; the order itself is server state.
const favorites = new Collection<ContactItem>({
  seed: () => Array.from({ length: 80 }, (_, index) => generateContact(index)),
});

export function fetchFavorites(signal?: RequestSignal): Promise<ContactItem[]> {
  return request(() => favorites.all(), signal);
}

export function saveFavoritesOrder(ids: ReadonlyArray<string>): Promise<void> {
  return request(() => favorites.reorder(ids));
}
