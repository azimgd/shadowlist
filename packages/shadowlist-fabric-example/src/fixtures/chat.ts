import type { ChatAuthor, ChatMessage } from 'shadowlist-utils/native';
import {
  AVATAR_NAMES,
  generateImageGrid,
  generateOptimizedImageUrl,
  generateRandomText,
  generateUniqueId,
  shouldBeImageGrid,
} from './common';

export const CHAT_ME: ChatAuthor = { id: 'me', name: 'Me' };

export function buildChatMessage(index: number): ChatMessage {
  if (index % 3 !== 0) {
    return buildMessageContent(index, CHAT_ME, true);
  }
  const name = AVATAR_NAMES[index % AVATAR_NAMES.length]!;
  return buildMessageContent(index, { id: name, name }, false);
}

function buildMessageContent(
  index: number,
  author: ChatAuthor,
  isOwn: boolean
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
    createdAt: Date.now(),
  };
}
