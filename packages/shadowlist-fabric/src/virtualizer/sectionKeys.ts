/*
 * The row keys of a sectioned list, the scheme host/ListSections.hpp defines: a header row is
 * HEADER_KEY_PREFIX and its section key, a footer row FOOTER_KEY_PREFIX and its section key, and
 * an item row ITEM_KEY_PREFIX, the section key, ITEM_KEY_SEPARATOR and the item key. An item
 * key may then repeat across sections. Native builds every row's key. JS builds the same keys
 * only for the rows it mounts, and for every row when the native store is missing.
 */
export const HEADER_KEY_PREFIX = '\x1fh:';
export const FOOTER_KEY_PREFIX = '\x1ff:';
export const ITEM_KEY_PREFIX = '\x1fi:';
export const ITEM_KEY_SEPARATOR = '\x1f';

export function headerRowKey(sectionKey: string): string {
  return HEADER_KEY_PREFIX + sectionKey;
}

export function footerRowKey(sectionKey: string): string {
  return FOOTER_KEY_PREFIX + sectionKey;
}

export function itemRowKey(sectionKey: string, itemKey: string): string {
  return ITEM_KEY_PREFIX + sectionKey + ITEM_KEY_SEPARATOR + itemKey;
}
