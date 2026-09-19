import type { SnapItem } from 'shadowlist-utils/native';
import { AVATAR_COLORS, generateUniqueId } from './common';

export function generateSnapElement(index: number): SnapItem {
  return {
    id: generateUniqueId(),
    color: AVATAR_COLORS[index % AVATAR_COLORS.length]!,
  };
}
