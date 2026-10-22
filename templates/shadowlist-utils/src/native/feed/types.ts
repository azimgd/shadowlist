export interface FeedAuthor {
  name: string;
  handle?: string;
  avatarUrl?: string;
  avatarColor?: string;
}

/*
 * color is drawn behind the image while it loads, and alone in a placeholder row.
 */
export interface FeedImage {
  uri: string;
  width?: number;
  height?: number;
  alt?: string;
  color?: string;
}

/*
 * A placeholder item stands in for a post still loading. Its strings are drawn as bars and its
 * images as coloured frames of their size. The row is the height the real post will be.
 */
export interface FeedItem {
  id: string;
  author: FeedAuthor;
  text?: string;
  images?: ReadonlyArray<FeedImage>;
  createdAt?: Date | number;
  placeholder?: boolean;
}
