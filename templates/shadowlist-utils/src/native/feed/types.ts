export interface FeedAuthor {
  name: string;
  handle?: string;
  avatarUrl?: string;
  avatarColor?: string;
}

export interface FeedImage {
  uri: string;
  width?: number;
  height?: number;
  alt?: string;
  // Drawn behind the image while it loads, and alone in a placeholder row.
  color?: string;
}

export interface FeedItem {
  id: string;
  author: FeedAuthor;
  text?: string;
  images?: ReadonlyArray<FeedImage>;
  createdAt?: Date | number;
  /*
   * A stand-in for a post still loading. Its strings are drawn as bars and its images as
   * coloured frames of their size, so the row is the height the real post will be.
   */
  placeholder?: boolean;
}
