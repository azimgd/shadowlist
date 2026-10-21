import {
  decodePost,
  encodePost,
  placeholderText,
  type HashEntry,
} from 'shadowhash';
import type { FeedItem } from 'shadowlist-utils/native';

/*
 * Both ends of a feed post's hash. A server would run hashFeedItem when a post is written and
 * send the hash ahead of the post. Here the fake API and the device cache run it.
 */
export function hashFeedItem(item: FeedItem): HashEntry {
  return {
    id: item.id,
    hash: encodePost({
      author: item.author,
      text: item.text,
      images: item.images,
    }),
  };
}

// Stand-in letters for a name or handle of this many characters.
const word = (length: number) =>
  placeholderText({ kind: 'words', tokens: [length] });

export function feedItemFromHash({ id, hash }: HashEntry): FeedItem {
  const shape = decodePost(hash);
  return {
    id,
    placeholder: true,
    author: {
      name: word(shape.author?.name ?? 8),
      ...(shape.author?.handle ? { handle: word(shape.author.handle) } : {}),
    },
    ...(shape.text ? { text: placeholderText(shape.text) } : {}),
    images: shape.images.map(({ aspectRatio, color }) => ({
      uri: '',
      ...(aspectRatio === undefined
        ? {}
        : { width: Math.round(aspectRatio * 1000), height: 1000 }),
      ...(color === undefined ? {} : { color }),
    })),
  };
}
