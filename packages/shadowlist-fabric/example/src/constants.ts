import type { FeedElement } from './FeedElement';
import type { NestedElement, NestedElementChild } from './NestedElement';
import type { MasonryElement } from './MasonryElement';
import type { ContactElement } from './ContactElement';
import {
  IMAGES,
  IMAGE_TITLES,
  SECTION_TITLES,
  SAMPLE_TEXTS,
  CHARACTER_NAMES,
  generateUniqueId,
  optimizeImageUrl,
} from 'shadowlist-utils';

/*
 * Shared, platform-agnostic data and helpers now live in shadowlist-utils and are
 * re-exported here so existing screen imports (./constants) keep working. Only the
 * generators that build this example's element shapes stay local.
 */
export * from 'shadowlist-utils';

export function generateFeedElement(index: number): FeedElement {
  const characterName = CHARACTER_NAMES[index % CHARACTER_NAMES.length]!;
  const handle = characterName.toLowerCase().replace(/\s+/g, '');

  const hasMultipleImages = index % 10 === 0;
  const imageCount = hasMultipleImages ? 3 + (index % 2) : 1;

  const imageUrls: string[] = [];
  for (let i = 0; i < imageCount; i++) {
    const imageIndex = (index + i) % IMAGES.length;
    const originalImageUrl = IMAGES[imageIndex]!;
    imageUrls.push(optimizeImageUrl(originalImageUrl, 800));
  }

  return {
    id: generateUniqueId(),
    username: characterName,
    handle: `@${handle}`,
    text: SAMPLE_TEXTS[index % SAMPLE_TEXTS.length]!,
    imageUrls,
    timestamp: `${Math.floor(Math.random() * 24)}h`,
  };
}

export function generateNestedElementChild(imageIndex: number): NestedElementChild {
  const originalImageUrl = IMAGES[imageIndex % IMAGES.length]!;

  return {
    id: generateUniqueId(),
    imageUrl: optimizeImageUrl(originalImageUrl, 400),
    title: IMAGE_TITLES[imageIndex % IMAGE_TITLES.length]!,
  };
}

export function generateNestedElement(rowIndex: number): NestedElement {
  const elementsPerRow = 10;
  const elements: NestedElementChild[] = [];

  for (let i = 0; i < elementsPerRow; i++) {
    elements.push(generateNestedElementChild(rowIndex * elementsPerRow + i));
  }

  return {
    id: generateUniqueId(),
    title: SECTION_TITLES[rowIndex % SECTION_TITLES.length]!,
    elements,
  };
}

const MASONRY_HEIGHTS = [180, 220, 260, 200, 240, 280, 190, 230, 250, 210];

export function generateMasonryElement(index: number): MasonryElement {
  const originalImageUrl = IMAGES[index % IMAGES.length]!;

  return {
    id: generateUniqueId(),
    imageUrl: optimizeImageUrl(originalImageUrl, 400),
    title: IMAGE_TITLES[index % IMAGE_TITLES.length]!,
    height: MASONRY_HEIGHTS[index % MASONRY_HEIGHTS.length]!,
  };
}

export function generateContact(index: number): ContactElement {
  const characterName = CHARACTER_NAMES[index % CHARACTER_NAMES.length]!;
  const nameParts = characterName.split(' ');

  const firstName = nameParts[0]!;
  const lastName = nameParts.length > 1 ? nameParts.slice(1).join(' ') : '';

  const areaCode = 100 + (index % 900);
  const exchange = 200 + (index % 800);
  const lineNumber = 1000 + (index % 9000);
  const phoneNumber = `(${areaCode}) ${exchange}-${lineNumber}`;

  return {
    id: generateUniqueId(),
    firstName,
    lastName,
    phoneNumber,
  };
}
