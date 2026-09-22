export interface FeedAuthor {
  name: string;
  handle?: string;
  avatarUrl?: string;
  // Falls back to a color derived from name.
  avatarColor?: string;
}

export interface FeedImage {
  uri: string;
  // Natural size. When both are known the image keeps its aspect ratio.
  width?: number;
  height?: number;
  alt?: string;
}

export interface FeedItem {
  id: string;
  author: FeedAuthor;
  text?: string;
  images?: ReadonlyArray<FeedImage>;
  createdAt?: Date | number;
}
