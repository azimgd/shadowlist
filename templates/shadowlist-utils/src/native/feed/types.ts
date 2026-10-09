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
}

export interface FeedItem {
  id: string;
  author: FeedAuthor;
  text?: string;
  images?: ReadonlyArray<FeedImage>;
  createdAt?: Date | number;
}
