import type { FeedImage, FeedItem } from 'shadowlist-utils/native';
import {
  CHARACTER_NAMES,
  IMAGES,
  IMAGE_TITLES,
  SAMPLE_TEXTS,
  generateUniqueId,
  optimizeImageUrl,
} from './common';

const HOUR_MS = 60 * 60 * 1000;

export function generateFeedElement(index: number): FeedItem {
  const name = CHARACTER_NAMES[index % CHARACTER_NAMES.length]!;
  const imageCount = index % 10 === 0 ? 3 + (index % 2) : 1;

  const images: FeedImage[] = Array.from({ length: imageCount }, (_, i) => {
    const imageIndex = (index + i) % IMAGES.length;
    return {
      uri: optimizeImageUrl(IMAGES[imageIndex]!, 800),
      alt: IMAGE_TITLES[imageIndex % IMAGE_TITLES.length],
    };
  });

  return {
    id: generateUniqueId(),
    author: {
      name,
      handle: `@${name.toLowerCase().replace(/\s+/g, '')}`,
    },
    text: SAMPLE_TEXTS[index % SAMPLE_TEXTS.length]!,
    images,
    createdAt: new Date(Date.now() - Math.floor(Math.random() * 24) * HOUR_MS),
  };
}
