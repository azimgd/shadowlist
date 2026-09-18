import type { NestedCardItem, NestedItem } from 'shadowlist-utils/native';
import {
  IMAGES,
  IMAGE_TITLES,
  SECTION_TITLES,
  generateUniqueId,
  optimizeImageUrl,
} from './common';

const CARDS_PER_ROW = 10;

export function generateNestedCard(imageIndex: number): NestedCardItem {
  return {
    id: generateUniqueId(),
    title: IMAGE_TITLES[imageIndex % IMAGE_TITLES.length]!,
    image: { uri: optimizeImageUrl(IMAGES[imageIndex % IMAGES.length]!, 400) },
  };
}

export function generateNestedElement(rowIndex: number): NestedItem {
  return {
    id: generateUniqueId(),
    title: SECTION_TITLES[rowIndex % SECTION_TITLES.length]!,
    cards: Array.from({ length: CARDS_PER_ROW }, (_, i) =>
      generateNestedCard(rowIndex * CARDS_PER_ROW + i)
    ),
  };
}
