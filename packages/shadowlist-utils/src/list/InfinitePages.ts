export interface ItemsPage<ItemT> {
  items: ReadonlyArray<ItemT>;
}

export interface InfinitePages<PageT> {
  pages: PageT[];
  pageParams: unknown[];
}

/**
 * The row type of an {@linkcode InfinitePages} value, e.g. `Post` for
 * `InfiniteData<{ items: Post[]; nextCursor?: number }>`.
 */
export type InfiniteItem<DataT> =
  DataT extends InfinitePages<ItemsPage<infer ItemT>> ? ItemT : never;
