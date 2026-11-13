import type { FeedImage } from './types';

export const SINGLE_IMAGE_HEIGHT = 200;
export const MIN_ASPECT_RATIO = 3 / 4;
export const MAX_ASPECT_RATIO = 16 / 9;

/*
 * The frame of a post's single image: its own aspect kept between 3:4 and 16:9, or
 * SINGLE_IMAGE_HEIGHT tall when the image has no size. feedSingleImageFrame in the core's
 * utils/FeedImageFrame.hpp is the same rule, and vectors.test holds the two equal.
 */
export function getSingleImageFrame(
  image: FeedImage
): { aspectRatio: number } | { height: number } {
  const ratio = image.width && image.height ? image.width / image.height : NaN;
  // A ratio that is not a number, like the one of two infinite sides, frames nothing.
  if (!Number.isNaN(ratio)) {
    return {
      aspectRatio: Math.min(
        MAX_ASPECT_RATIO,
        Math.max(MIN_ASPECT_RATIO, ratio)
      ),
    };
  }
  return { height: SINGLE_IMAGE_HEIGHT };
}
