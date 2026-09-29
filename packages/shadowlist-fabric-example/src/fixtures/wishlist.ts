import type { ReorderTileItem } from 'shadowlist-utils/native';
import { IMAGE_TITLES, generateUniqueId } from './common';

// Swatch width over height for the mixed sizes layout.
const ASPECT_RATIOS = [1, 0.8, 1.3, 0.9, 1.2, 0.7, 1.1];

export function generateWishlistItem(index: number): ReorderTileItem {
  return {
    id: generateUniqueId(),
    title: IMAGE_TITLES[index % IMAGE_TITLES.length]!,
    label: String(index + 1),
    aspectRatio: ASPECT_RATIOS[index % ASPECT_RATIOS.length]!,
  };
}
