import type { HashEntry } from 'shadowhash';
import type { FeedItem } from 'shadowlist-utils/native';
import { generateFeedElement } from '../fixtures/feed';
import { Collection, type CursorPage, type PageCursor } from './Collection';
import { request, type RequestSignal } from './network';
import { listCount } from '../launchSettings';
import { hashFeedItem } from '../shadowhash/feedHash';

const PAGE_SIZE = 20;
/*
 * With SLCount the first page holds exactly that many posts.
 */
const FIRST_PAGE_SIZE = listCount;
/*
 * Posts other people publish between two pulls to refresh.
 */
const NEW_POSTS_PER_REFRESH = 10;

let generatedCount = 0;
const generatePosts = (count: number) =>
  Array.from({ length: count }, () => generateFeedElement(generatedCount++));

const posts = new Collection<FeedItem>({
  seed: () => generatePosts(Math.max(1000, FIRST_PAGE_SIZE)),
  extend: generatePosts,
});

let headReads = 0;

/*
 * A page of posts with the hashes of the page after it, about 30 bytes a post. The list draws
 * that page's rows at their real size while it loads.
 */
export interface FeedPage extends CursorPage<FeedItem> {
  upcoming: HashEntry[];
}

export function fetchFeedPage(
  cursor: PageCursor | undefined,
  signal?: RequestSignal
): Promise<FeedPage> {
  return request(() => {
    // The first read of the head sees the seeded feed; every later one finds newer posts.
    if (cursor === undefined && headReads++ > 0) {
      posts.insertAtStart(generatePosts(NEW_POSTS_PER_REFRESH));
    }
    const page = posts.page(cursor, {
      limit: cursor === undefined ? FIRST_PAGE_SIZE : PAGE_SIZE,
      from: 'start',
    });
    const upcoming =
      page.nextCursor === undefined
        ? []
        : posts
            .page(
              { after: page.nextCursor },
              { limit: PAGE_SIZE, from: 'start' }
            )
            .items.map(hashFeedItem);
    return { ...page, upcoming };
  }, signal);
}

export function publishPosts(count: number): Promise<FeedItem[]> {
  return request(() => {
    const created = generatePosts(count);
    posts.insertAtStart(created);
    return created;
  });
}
