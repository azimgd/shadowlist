import type { FeedItem } from 'shadowlist-utils/native';
import { generateFeedElement } from '../fixtures/feed';
import { Collection, type CursorPage, type PageCursor } from './Collection';
import { request, type RequestSignal } from './network';

const PAGE_SIZE = 20;
// Posts other people publish between two pulls to refresh.
const NEW_POSTS_PER_REFRESH = 10;

let generatedCount = 0;
const generatePosts = (count: number) =>
  Array.from({ length: count }, () => generateFeedElement(generatedCount++));

const posts = new Collection<FeedItem>({
  seed: () => generatePosts(1000),
  extend: generatePosts,
});

let headReads = 0;

export function fetchFeedPage(
  cursor: PageCursor | undefined,
  signal?: RequestSignal
): Promise<CursorPage<FeedItem>> {
  return request(() => {
    // The first read of the head sees the seeded feed; every later one finds newer posts.
    if (cursor === undefined && headReads++ > 0) {
      posts.insertAtStart(generatePosts(NEW_POSTS_PER_REFRESH));
    }
    return posts.page(cursor, { limit: PAGE_SIZE, from: 'start' });
  }, signal);
}

export function publishPosts(count: number): Promise<FeedItem[]> {
  return request(() => {
    const created = generatePosts(count);
    posts.insertAtStart(created);
    return created;
  });
}
