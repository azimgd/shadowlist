import { FeedList } from './FeedList';
import { FeedElement } from './FeedElement';

export type { FeedListProps } from './FeedList';
export type { FeedElementProps } from './FeedElement';

// Feed domain: <Feed.List data={posts} /> + <Feed.Element />.
export const Feed = {
  List: FeedList,
  Element: FeedElement,
};
