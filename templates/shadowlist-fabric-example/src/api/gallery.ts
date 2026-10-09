import type {
  MasonryItem,
  NestedItem,
  ReorderTileItem,
} from 'shadowlist-utils/native';
import { generateMasonryElement } from '../fixtures/masonry';
import { generateNestedElement } from '../fixtures/nested';
import { generateWishlistItem } from '../fixtures/wishlist';
import { Collection, type CursorPage, type PageCursor } from './Collection';
import { request, type RequestSignal } from './network';
import { listCount } from '../launchSettings';

/*
 * Photos for the Masonry grid.
 */
const PHOTO_PAGE_SIZE = 30;
/*
 * With SLCount the first page holds exactly that many photos.
 */
const FIRST_PHOTO_PAGE_SIZE = listCount;

let photoCount = 0;
const generatePhotos = (count: number) =>
  Array.from({ length: count }, () => generateMasonryElement(photoCount++));

const photos = new Collection<MasonryItem>({
  seed: () => generatePhotos(Math.max(100, FIRST_PHOTO_PAGE_SIZE)),
  extend: generatePhotos,
});

export function fetchPhotosPage(
  cursor: PageCursor | undefined,
  signal?: RequestSignal
): Promise<CursorPage<MasonryItem>> {
  return request(
    () =>
      photos.page(cursor, {
        limit: cursor === undefined ? FIRST_PHOTO_PAGE_SIZE : PHOTO_PAGE_SIZE,
        from: 'start',
      }),
    signal
  );
}

export function publishPhotos(count: number): Promise<MasonryItem[]> {
  return request(() => {
    const created = generatePhotos(count);
    photos.insertAtStart(created);
    return created;
  });
}

/*
 * Shelves for the Nested screen, each a horizontal carousel.
 */
const SHELF_PAGE_SIZE = 10;

let shelfCount = 0;
const generateShelves = (count: number) =>
  Array.from({ length: count }, () => generateNestedElement(shelfCount++));

const shelves = new Collection<NestedItem>({
  seed: () => generateShelves(20),
  extend: generateShelves,
});

export function fetchShelvesPage(
  cursor: PageCursor | undefined,
  signal?: RequestSignal
): Promise<CursorPage<NestedItem>> {
  return request(
    () => shelves.page(cursor, { limit: SHELF_PAGE_SIZE, from: 'start' }),
    signal
  );
}

/*
 * A user-ordered wishlist of sights for the reorder grid; the order itself is server state.
 */
const wishlist = new Collection<ReorderTileItem>({
  seed: () =>
    Array.from({ length: 60 }, (_, index) => generateWishlistItem(index)),
});

export function fetchWishlist(
  signal?: RequestSignal
): Promise<ReorderTileItem[]> {
  return request(() => wishlist.all(), signal);
}

export function saveWishlistOrder(ids: ReadonlyArray<string>): Promise<void> {
  return request(() => wishlist.reorder(ids));
}
