export interface ItemsPage<ItemT> {
  items: ReadonlyArray<ItemT>;
}

export interface InfinitePages<PageT> {
  pages: PageT[];
  pageParams: unknown[];
}

/**
 * The row type inside an {@linkcode InfinitePages} value, like `Post` for
 * `InfiniteData<{ items: Post[]; nextCursor?: number }>`.
 */
export type InfiniteItem<DataT> =
  DataT extends InfinitePages<ItemsPage<infer ItemT>> ? ItemT : never;
