import type { MasonryItem } from 'shadowlist-utils/native';
import {
  IMAGES,
  IMAGE_TITLES,
  generateUniqueId,
  optimizeImageUrl,
} from './common';

const IMAGE_WIDTH = 400;
// Card heights (pt) the grid was designed with, at a ~122pt image width.
const DESIGN_HEIGHTS = [180, 220, 260, 200, 240, 280, 190, 230, 250, 210];
const DESIGN_WIDTH = 122;

export function generateMasonryElement(index: number): MasonryItem {
  const designHeight = DESIGN_HEIGHTS[index % DESIGN_HEIGHTS.length]!;
  return {
    id: generateUniqueId(),
    title: IMAGE_TITLES[index % IMAGE_TITLES.length]!,
    image: {
      uri: optimizeImageUrl(IMAGES[index % IMAGES.length]!, IMAGE_WIDTH),
      width: IMAGE_WIDTH,
      height: Math.round((IMAGE_WIDTH * designHeight) / DESIGN_WIDTH),
    },
  };
}
