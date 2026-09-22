export interface ChatAuthor {
  id: string;
  name: string;
  avatarUrl?: string;
  // Derived from name when left out.
  avatarColor?: string;
}

/*
 * Delivery state of the reader's own message. One field instead of a flag per state, so a
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
  // One image renders full size; two or more render as a grid of up to four.
  images?: ReadonlyArray<string>;
  createdAt?: number;
  status?: ChatMessageStatus;
}
