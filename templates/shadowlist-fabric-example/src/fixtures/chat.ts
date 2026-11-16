import type { ChatAuthor, ChatMessage } from 'shadowlist-utils/native';
import { listCount } from '../launchSettings';
import {
  FIXTURE_NOW,
  AVATAR_NAMES,
  generateImageGrid,
  generateOptimizedImageUrl,
  generateRandomText,
  generateUniqueId,
  shouldBeImageGrid,
} from './common';

export const CHAT_ME: ChatAuthor = { id: 'me', name: 'Me' };

export function buildChatMessage(
  index: number,
  count: number = listCount
): ChatMessage {
  if (index % 3 !== 0) {
    return buildMessageContent(index, CHAT_ME, true, count);
  }
  const name = AVATAR_NAMES[index % AVATAR_NAMES.length]!;
  return buildMessageContent(index, { id: name, name }, false, count);
}

function buildMessageContent(
  index: number,
  author: ChatAuthor,
  isOwn: boolean,
  count: number
): ChatMessage {
  const imageUrl = generateOptimizedImageUrl(index);
  const images = shouldBeImageGrid(index)
    ? generateImageGrid(index)
    : imageUrl !== undefined
      ? [imageUrl]
      : undefined;
  return {
    id: generateUniqueId(),
    author,
    isOwn,
    text: images === undefined ? generateRandomText(index) : undefined,
    images,
    createdAt: FIXTURE_NOW - Math.max(0, count - index) * 60_000,
  };
}
