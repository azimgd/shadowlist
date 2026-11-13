import {
  groupIntoSections,
  type ItemSection,
} from '../../list/groupIntoSections';
import type { ContactItem } from './types';

/*
 * The title of the section for names that start with no letter. It sorts after every other
 * section.
 */
export const OTHER_SECTION_TITLE = '#';

/*
 * The title of the section a contact's name falls in: the first character of the name after
 * trimming, in upper case, or OTHER_SECTION_TITLE when the name is blank or that character is
 * no letter. sectionInitial in the core's utils/ContactSection.hpp is the same rule, and
 * vectors.test holds them equal.
 */
export function getContactSectionTitle(name: string): string {
  const codePoint = name.trim().codePointAt(0);
  if (codePoint === undefined) {
    return OTHER_SECTION_TITLE;
  }
  const first = String.fromCodePoint(codePoint);
  return /\p{L}/u.test(first) ? first.toUpperCase() : OTHER_SECTION_TITLE;
}

function compareSectionTitles(first: string, second: string): number {
  if (first === OTHER_SECTION_TITLE || second === OTHER_SECTION_TITLE) {
    return first === second ? 0 : first === OTHER_SECTION_TITLE ? 1 : -1;
  }
  return first.localeCompare(second);
}

/*
 * Contacts grouped into the sections Contacts.SectionList takes, by getContactSectionTitle.
 * Sections sort by title in the reader's locale with OTHER_SECTION_TITLE last. Contacts keep
 * their order inside a section, and a section's key is its title.
 */
export function groupContactsIntoSections(
  contacts: ReadonlyArray<ContactItem>
): ItemSection<ContactItem>[] {
  return groupIntoSections(contacts, {
    getSectionTitle: (contact) => getContactSectionTitle(contact.name),
    compareSections: compareSectionTitles,
  });
}
