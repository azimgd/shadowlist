import type { InfiniteItem, InfinitePages, ItemsPage } from './InfinitePages';

/*
 * Row edits for cached infinite data, made for optimistic updates, for example
 *
 *   queryClient.setQueryData<FeedData>(['feed'], (data) =>
 *     removeInfiniteItems(data, (post) => post.id === deletedId)
 *   );
 *
 * Pages and rows that did not change keep their identity, and data comes back as is when
 * nothing changed, so memoized rows skip the re-render. When nothing is cached yet undefined
 * passes through, which React Query takes as leave the cache alone.
 */

type AnyInfinitePages = InfinitePages<ItemsPage<unknown>>;

const NO_ITEMS: ReadonlyArray<never> = Object.freeze([]);

function mapPages<DataT extends AnyInfinitePages>(
  data: DataT,
  mapItems: (
    items: ReadonlyArray<unknown>,
    pageIndex: number,
    pageCount: number
  ) => ReadonlyArray<unknown>
): DataT {
  let changed = false;
  const pages = data.pages.map((page, pageIndex) => {
    const items = mapItems(page.items, pageIndex, data.pages.length);
    if (items === page.items) return page;
    changed = true;
    return { ...page, items };
  });
  return changed ? ({ ...data, pages } as DataT) : data;
}

/**
 * Every row of every loaded page in display order, ready for the list's `data` prop.
 * Returns one shared empty array while nothing is cached, so it is safe as a memo dependency.
 *
 * @see {@linkcode useInfiniteListProps}, which memoizes this for you.
 */
export function flattenInfiniteItems<DataT extends AnyInfinitePages>(
  data: DataT | undefined
): ReadonlyArray<InfiniteItem<DataT>> {
  if (data === undefined) return NO_ITEMS;
  const pages = data.pages as ReadonlyArray<ItemsPage<InfiniteItem<DataT>>>;
  if (pages.length === 1) return pages[0]!.items;
  return pages.flatMap((page) => page.items);
}

/**
 * Replaces rows through `update`. Return the row itself to leave it unchanged.
 *
 * @example
 * updateInfiniteItems(data, (option) =>
 *   option.id === votedId ? { ...option, votes: option.votes + 1 } : option
 * );
 */
export function updateInfiniteItems<DataT extends AnyInfinitePages>(
  data: DataT | undefined,
  update: (item: InfiniteItem<DataT>) => InfiniteItem<DataT>
): DataT | undefined {
  if (data === undefined) return undefined;
  return mapPages(data, (items) => {
    let changed = false;
    const next = items.map((item) => {
      const updated = update(item as InfiniteItem<DataT>);
      if (updated !== item) changed = true;
      return updated;
    });
    return changed ? next : items;
  });
}

/** Removes every row `shouldRemove` matches, from whichever page holds it. */
export function removeInfiniteItems<DataT extends AnyInfinitePages>(
  data: DataT | undefined,
  shouldRemove: (item: InfiniteItem<DataT>) => boolean
): DataT | undefined {
  if (data === undefined) return undefined;
  return mapPages(data, (items) => {
    const kept = items.filter(
      (item) => !shouldRemove(item as InfiniteItem<DataT>)
    );
    return kept.length === items.length ? items : kept;
  });
}

/**
 * Inserts rows at the top of the first loaded page, like posts that were just published.
 * Only correct while the first loaded page is the real start of the collection. Otherwise
 * refetch instead.
 */
export function prependInfiniteItems<DataT extends AnyInfinitePages>(
  data: DataT | undefined,
  items: ReadonlyArray<InfiniteItem<DataT>>
): DataT | undefined {
  if (data === undefined || items.length === 0) return data;
  return mapPages(data, (pageItems, pageIndex) =>
    pageIndex === 0 ? [...items, ...pageItems] : pageItems
  );
}

/**
 * Inserts rows at the bottom of the last loaded page, like a sent chat message. Only
 * correct while there is no next page. Otherwise the rows end up between that page and the
 * next one once it loads.
 */
export function appendInfiniteItems<DataT extends AnyInfinitePages>(
  data: DataT | undefined,
  items: ReadonlyArray<InfiniteItem<DataT>>
): DataT | undefined {
  if (data === undefined || items.length === 0) return data;
  return mapPages(data, (pageItems, pageIndex, pageCount) =>
    pageIndex === pageCount - 1 ? [...pageItems, ...items] : pageItems
  );
}

/**
 * Replaces each row whose `id` is already cached, wherever it sits, and appends the rest to
 * the last loaded page, with the same catch as {@linkcode appendInfiniteItems}. Use it to
 * confirm or fail an optimistic send, or for a socket that may send a message twice. An id
 * that is already cached never turns into a duplicate key.
 *
 * @example
 * upsertInfiniteItems(data, [{ ...message, status: 'failed' }]);
 */
export function upsertInfiniteItems<DataT extends AnyInfinitePages>(
  data: DataT | undefined,
  items: ReadonlyArray<InfiniteItem<DataT> & { id: string }>
): DataT | undefined {
  if (data === undefined || items.length === 0) return data;
  const pending = new Map(items.map((item) => [item.id, item]));
  const replaced = mapPages(data, (pageItems) => {
    let changed = false;
    const next = pageItems.map((item) => {
      const id = (item as { id?: unknown }).id;
      const replacement = typeof id === 'string' ? pending.get(id) : undefined;
      if (replacement === undefined) return item;
      pending.delete(id as string);
      if (replacement === item) return item;
      changed = true;
      return replacement;
    });
    return changed ? next : pageItems;
  });
  return appendInfiniteItems(replaced, Array.from(pending.values()));
}

/**
 * Keeps only the first `pageCount` loaded pages. A refetch loads every cached page one by
 * one, so after a long scroll a pull to refresh can take seconds. Trimming to the first
 * page makes it one request, and a reader pulling at the top never sees the dropped rows.
 */
export function trimInfinitePages<DataT extends AnyInfinitePages>(
  data: DataT | undefined,
  pageCount: number
): DataT | undefined {
  if (data === undefined || data.pages.length <= pageCount) return data;
  return {
    ...data,
    pages: data.pages.slice(0, pageCount),
    pageParams: data.pageParams.slice(0, pageCount),
  };
}
