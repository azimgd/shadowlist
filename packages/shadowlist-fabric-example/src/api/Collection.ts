/*
 * An in-memory table standing in for a server-side collection. Rows are kept in display
 * order, each with a numeric cursor that grows toward the end and shrinks toward the start.
 * A cursor stays valid while rows are inserted or deleted around it, so paging never skips
 * or repeats a row and the list never receives a duplicate key -- which offset pagination
 * would do the moment someone publishes above the page a reader has loaded.
 */

// Where a page starts: right after a row's cursor, or right before it.
export type PageCursor = { after: number } | { before: number };

export interface CursorPage<ItemT> {
  items: ItemT[];
  nextCursor?: number;
  previousCursor?: number;
}

// getNextPageParam / getPreviousPageParam for any CursorPage endpoint.
export function nextPageCursor(
  page: CursorPage<unknown>
): PageCursor | undefined {
  return page.nextCursor === undefined ? undefined : { after: page.nextCursor };
}

export function previousPageCursor(
  page: CursorPage<unknown>
): PageCursor | undefined {
  return page.previousCursor === undefined
    ? undefined
    : { before: page.previousCursor };
}

interface Row<ItemT> {
  cursor: number;
  item: ItemT;
}

interface CollectionOptions<ItemT> {
  seed: () => ItemT[];
  extend?: (count: number) => ItemT[];
}

export class Collection<ItemT extends { id: string }> {
  private rows: Row<ItemT>[] | undefined;
  private readonly options: CollectionOptions<ItemT>;

  constructor(options: CollectionOptions<ItemT>) {
    this.options = options;
  }

  private table(): Row<ItemT>[] {
    if (!this.rows) {
      this.rows = this.options
        .seed()
        .map((item, index) => ({ cursor: index, item }));
    }
    return this.rows;
  }

  // First index whose row matches, or the row count when none does.
  private indexWhere(match: (row: Row<ItemT>) => boolean): number {
    const index = this.table().findIndex(match);
    return index === -1 ? this.table().length : index;
  }

  all(): ItemT[] {
    return this.table().map((row) => row.item);
  }

  /*
   * One page. Without a cursor it is the first `limit` rows (`from: 'start'`, a feed) or the
   * last `limit` rows (`from: 'end'`, a chat opening on its newest messages).
   */
  page(
    cursor: PageCursor | undefined,
    { limit, from }: { limit: number; from: 'start' | 'end' }
  ): CursorPage<ItemT> {
    const { extend } = this.options;
    if (extend && cursor && 'after' in cursor) {
      const start = this.indexWhere((row) => row.cursor > cursor.after);
      const remaining = this.table().length - start;
      if (remaining < limit) this.insertAtEnd(extend(limit - remaining));
    }

    const rows = this.table();
    let start: number;
    let end: number;
    if (cursor === undefined) {
      start = from === 'start' ? 0 : Math.max(0, rows.length - limit);
      end = Math.min(rows.length, start + limit);
    } else if ('after' in cursor) {
      start = this.indexWhere((row) => row.cursor > cursor.after);
      end = Math.min(rows.length, start + limit);
    } else {
      end = this.indexWhere((row) => row.cursor >= cursor.before);
      start = Math.max(0, end - limit);
    }

    const hasRows = end > start;
    // An extendable collection always has more past its end, even when a page stops exactly there.
    const hasMoreAfter = end < rows.length || extend !== undefined;
    return {
      items: rows.slice(start, end).map((row) => row.item),
      nextCursor: hasRows && hasMoreAfter ? rows[end - 1]!.cursor : undefined,
      previousCursor: hasRows && start > 0 ? rows[start]!.cursor : undefined,
    };
  }

  insertAtStart(items: ItemT[]): void {
    const rows = this.table();
    const first = rows[0]?.cursor ?? 0;
    rows.unshift(
      ...items.map((item, index) => ({
        cursor: first - items.length + index,
        item,
      }))
    );
  }

  insertAtEnd(items: ItemT[]): void {
    const rows = this.table();
    const last = rows[rows.length - 1]?.cursor ?? -1;
    rows.push(
      ...items.map((item, index) => ({ cursor: last + 1 + index, item }))
    );
  }

  remove(ids: ReadonlyArray<string>): void {
    const removed = new Set(ids);
    this.rows = this.table().filter((row) => !removed.has(row.item.id));
  }

  // The updated row, or undefined when `id` is gone.
  update(id: string, update: (item: ItemT) => ItemT): ItemT | undefined {
    const row = this.table().find((candidate) => candidate.item.id === id);
    if (!row) return undefined;
    row.item = update(row.item);
    return row.item;
  }

  // Rewrites the order to `ids`, which lists every row.
  reorder(ids: ReadonlyArray<string>): void {
    const byId = new Map(this.table().map((row) => [row.item.id, row.item]));
    this.rows = ids.flatMap((id, index) => {
      const item = byId.get(id);
      return item ? [{ cursor: index, item }] : [];
    });
  }
}
