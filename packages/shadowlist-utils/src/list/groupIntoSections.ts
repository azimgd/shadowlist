export interface ItemSection<ItemT> {
  key: string;
  title: string;
  data: ItemT[];
}

export interface GroupIntoSectionsOptions<ItemT> {
  getSectionTitle: (item: ItemT) => string;
  compareSections?: (a: string, b: string) => number;
  compareItems?: (a: ItemT, b: ItemT) => number;
}

const byLocale = (a: string, b: string) => a.localeCompare(b);

/**
 * Groups a flat, server-ordered array into the `sections` a SectionList takes. Memoize the
 * result on the input array: a new `sections` value makes the list rebuild its section
 * index.
 *
 * @example
 * const sections = useMemo(
 *   () =>
 *     groupIntoSections(contacts, {
 *       getSectionTitle: (contact) => contact.name.charAt(0).toUpperCase(),
 *     }),
 *   [contacts]
 * );
 */
export function groupIntoSections<ItemT>(
  items: ReadonlyArray<ItemT>,
  {
    getSectionTitle,
    compareSections = byLocale,
    compareItems,
  }: GroupIntoSectionsOptions<ItemT>
): ItemSection<ItemT>[] {
  const groups = new Map<string, ItemT[]>();
  for (const item of items) {
    const title = getSectionTitle(item);
    const group = groups.get(title);
    if (group) group.push(item);
    else groups.set(title, [item]);
  }

  return Array.from(groups.keys())
    .sort(compareSections)
    .map((title) => {
      const data = groups.get(title)!;
      return {
        key: title,
        title,
        data: compareItems ? data.sort(compareItems) : data,
      };
    });
}
