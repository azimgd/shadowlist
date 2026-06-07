import { ChatList } from './ChatList';
import { ChatBubble } from './ChatBubble';
import { ChatInput } from './ChatInput';

export type { ChatListProps } from './ChatList';
export type { ChatBubbleProps, ChatMessage } from './ChatBubble';
export type { ChatInputProps } from './ChatInput';

// Chat domain: <Chat.List data={messages} /> + <Chat.Bubble /> + <Chat.Input />.
export const Chat = {
  List: ChatList,
  Bubble: ChatBubble,
  Input: ChatInput,
};
