import type { ContactItem } from 'shadowlist-utils/native';
import { CHARACTER_NAMES, generateUniqueId } from './common';

const FIRST_NAMES = CHARACTER_NAMES.map((name) => name.split(' ')[0]!);
const LAST_NAMES = CHARACTER_NAMES.map((name) => name.split(' ')[1]!);

function contactName(index: number): string {
  const count = CHARACTER_NAMES.length;
  const first = FIRST_NAMES[index % count]!;
  const last = LAST_NAMES[(index * 7 + Math.floor(index / count)) % count]!;
  return `${first} ${last}`;
}

export function generateContact(index: number): ContactItem {
  const areaCode = 100 + (index % 900);
  const exchange = 200 + (index % 800);
  const lineNumber = 1000 + (index % 9000);

  return {
    id: generateUniqueId(),
    name: contactName(index),
    subtitle: `(${areaCode}) ${exchange}-${lineNumber}`,
  };
}
