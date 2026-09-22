import type { SnapItem } from 'shadowlist-utils/native';
import {
  AVATAR_COLORS,
  IMAGES,
  IMAGE_TITLES,
  generateUniqueId,
  optimizeImageUrl,
} from './common';

const IMAGE_WIDTH = 800;

export function generateSnapElement(index: number): SnapItem {
  const title = IMAGE_TITLES[index % IMAGE_TITLES.length]!;
  return {
    id: generateUniqueId(),
    color: AVATAR_COLORS[index % AVATAR_COLORS.length]!,
    title,
    image: {
      uri: optimizeImageUrl(IMAGES[index % IMAGES.length]!, IMAGE_WIDTH),
      alt: title,
    },
  };
}
