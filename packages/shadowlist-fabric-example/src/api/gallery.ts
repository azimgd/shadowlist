import type { MasonryItem, NestedItem } from 'shadowlist-utils/native';
import { generateMasonryElement } from '../fixtures/masonry';
import { generateNestedElement } from '../fixtures/nested';
import { Collection, type CursorPage, type PageCursor } from './Collection';
import { request, type RequestSignal } from './network';

// Photos: the Masonry grid.
const PHOTO_PAGE_SIZE = 30;

let photoCount = 0;
const generatePhotos = (count: number) =>
  Array.from({ length: count }, () => generateMasonryElement(photoCount++));

const photos = new Collection<MasonryItem>({
  seed: () => generatePhotos(100),
  extend: generatePhotos,
});

export function fetchPhotosPage(
  cursor: PageCursor | undefined,
  signal?: RequestSignal
): Promise<CursorPage<MasonryItem>> {
  return request(
    () => photos.page(cursor, { limit: PHOTO_PAGE_SIZE, from: 'start' }),
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

// Shelves: the Nested screen's rows, each a horizontal carousel.
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
