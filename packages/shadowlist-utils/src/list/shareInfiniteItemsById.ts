import type { InfinitePages, ItemsPage } from './InfinitePages';

type AnyInfinitePages = InfinitePages<ItemsPage<unknown>>;

function isInfinitePages(value: unknown): value is AnyInfinitePages {
  return (
    typeof value === 'object' &&
    value !== null &&
    Array.isArray((value as AnyInfinitePages).pages) &&
    Array.isArray((value as AnyInfinitePages).pageParams)
  );
}

function isPlainObject(value: unknown): value is Record<string, unknown> {
  if (typeof value !== 'object' || value === null) return false;
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}

// Value equality for JSON-like data: primitives, arrays and plain objects.
function deepEqual(a: unknown, b: unknown): boolean {
  if (Object.is(a, b)) return true;
  if (Array.isArray(a)) {
    if (!Array.isArray(b) || a.length !== b.length) return false;
    for (let index = 0; index < a.length; index++) {
      if (!deepEqual(a[index], b[index])) return false;
    }
    return true;
  }
  if (isPlainObject(a) && isPlainObject(b)) {
    const keys = Object.keys(a);
    if (keys.length !== Object.keys(b).length) return false;
    for (const key of keys) {
      if (!Object.prototype.hasOwnProperty.call(b, key)) return false;
      if (!deepEqual(a[key], b[key])) return false;
    }
    return true;
  }
  return false;
}

function idOf(item: unknown): unknown {
  return typeof item === 'object' && item !== null
    ? (item as { id?: unknown }).id
    : undefined;
}

// Whether two pages carry the same fields, with `items` compared by identity per row.
function samePage(
  previous: ItemsPage<unknown>,
  next: ItemsPage<unknown>,
  items: ReadonlyArray<unknown>
): boolean {
  if (previous.items.length !== items.length) return false;
  for (let index = 0; index < items.length; index++) {
    if (previous.items[index] !== items[index]) return false;
  }
  const previousKeys = Object.keys(previous);
  const nextKeys = Object.keys(next);
  if (previousKeys.length !== nextKeys.length) return false;
  for (const key of nextKeys) {
    if (key === 'items') continue;
    if (
      !deepEqual(
        (previous as unknown as Record<string, unknown>)[key],
        (next as unknown as Record<string, unknown>)[key]
      )
    ) {
      return false;
    }
  }
  return true;
}

// Rows of a plain (non-paginated) query, shared by id the same way.
function shareArrayById(
  previous: ReadonlyArray<unknown>,
  next: ReadonlyArray<unknown>
): ReadonlyArray<unknown> {
  const previousById = new Map<unknown, unknown>();
  for (const item of previous) {
    const id = idOf(item);
    if (id !== undefined && !previousById.has(id)) previousById.set(id, item);
  }

  let changed = previous.length !== next.length;
  const items = next.map((item, index) => {
    const id = idOf(item);
    const previousItem = id === undefined ? undefined : previousById.get(id);
    const shared =
      previousItem !== undefined &&
      previousItem !== item &&
      deepEqual(previousItem, item)
        ? previousItem
        : item;
    if (shared !== previous[index]) changed = true;
    return shared;
  });
  return changed ? items : previous;
}

/**
 * Structural sharing keyed by `id`, for a plain list query or an infinite one. Pass it as a
 * React Query `structuralSharing` option:
 *
 *   useQuery({ ..., structuralSharing: shareItemsById });
 *
 * The default compares arrays by position, so inserting or removing a row hands every row
 * after it a new object and re-renders the whole mounted window even though nothing in those
 * rows changed. Here a row equal to the previous row with the same id keeps its identity
 * wherever it moved.
 *
 * @see {@linkcode shareInfiniteItemsById} for the infinite-data-only version.
 */
export function shareItemsById<DataT>(previous: unknown, next: DataT): DataT {
  if (Array.isArray(previous) && Array.isArray(next)) {
    return shareArrayById(previous, next) as DataT;
  }
  return shareInfiniteItemsById(previous, next);
}

/**
 * Structural sharing for infinite data that matches rows by `id` rather than by position.
 * Pass it as a React Query `structuralSharing` option:
 *
 *   useInfiniteQuery({ ..., structuralSharing: shareInfiniteItemsById });
 *
 * The default (`replaceEqualDeep`) compares arrays index by index. Rows prepended to a page,
 * or a page of history loaded in front of the others, move every following row to a new
 * index, so each of those rows comes back as a fresh object although nothing in it changed:
 * every mounted row and every memoized cell re-renders, and the list pays for a full window
 * of renders on each prepend. Here a row that is value-equal to the previous row with the
 * same id keeps the previous object wherever it moved, a page whose rows and fields are all
 * unchanged keeps the previous page object, and data that did not change at all returns the
 * previous value itself, so observers do not re-render.
 *
 * Rows without an `id` and values that are not infinite data pass through unshared.
 */
export function shareInfiniteItemsById<DataT>(
  previous: unknown,
  next: DataT
): DataT {
  if (!isInfinitePages(previous) || !isInfinitePages(next)) return next;

  const previousItems = new Map<unknown, unknown>();
  const previousPagesByFirstId = new Map<unknown, ItemsPage<unknown>>();
  for (const page of previous.pages) {
    const firstId = idOf(page.items[0]);
    if (firstId !== undefined && !previousPagesByFirstId.has(firstId)) {
      previousPagesByFirstId.set(firstId, page);
    }
    for (const item of page.items) {
      const id = idOf(item);
      if (id !== undefined && !previousItems.has(id)) {
        previousItems.set(id, item);
      }
    }
  }

  let pagesChanged = next.pages.length !== previous.pages.length;
  const pages = next.pages.map((page, pageIndex) => {
    let itemsChanged = false;
    const items = page.items.map((item) => {
      const id = idOf(item);
      if (id === undefined) return item;
      const previousItem = previousItems.get(id);
      if (previousItem !== undefined && previousItem !== item) {
        if (deepEqual(previousItem, item)) {
          itemsChanged = true;
          return previousItem;
        }
      }
      return item;
    });
    const sharedItems = itemsChanged ? items : page.items;
    const previousPage = previousPagesByFirstId.get(idOf(sharedItems[0]));
    if (
      previousPage !== undefined &&
      samePage(previousPage, page, sharedItems)
    ) {
      if (previous.pages[pageIndex] !== previousPage) pagesChanged = true;
      return previousPage;
    }
    pagesChanged = true;
    return itemsChanged ? { ...page, items: sharedItems } : page;
  });

  const pageParams = deepEqual(previous.pageParams, next.pageParams)
    ? previous.pageParams
    : next.pageParams;
  if (!pagesChanged && pageParams === previous.pageParams) {
    const nextKeys = Object.keys(next).filter(
      (key) => key !== 'pages' && key !== 'pageParams'
    );
    const extraEqual = nextKeys.every((key) =>
      deepEqual(
        (previous as unknown as Record<string, unknown>)[key],
        (next as unknown as Record<string, unknown>)[key]
      )
    );
    if (extraEqual) return previous as unknown as DataT;
  }
  return { ...next, pages, pageParams } as DataT;
}
