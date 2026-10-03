import type { ColorValue } from 'react-native';

export interface ChatAuthor {
  id: string;
  name: string;
  avatarUrl?: string;
  avatarColor?: ColorValue;
}

/*
 * Delivery state of the reader's own message. One field instead of a flag per state. A
 * message can't be both sending and failed. Messages from others leave it unset.
 */
export type ChatMessageStatus =
  | 'sending'
  | 'sent'
  | 'delivered'
  | 'read'
  | 'failed';

export interface ChatMessage {
  id: string;
  author: ChatAuthor;
  isOwn: boolean;
  text?: string;
  images?: ReadonlyArray<string>;
  createdAt?: number;
  status?: ChatMessageStatus;
}
