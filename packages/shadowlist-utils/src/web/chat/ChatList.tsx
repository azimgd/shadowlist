import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist-wasm';
import { ChatBubble, type ChatMessage } from './ChatBubble';

export type ChatListProps = Omit<ShadowlistProps<ChatMessage>, 'renderElement'> & {
  renderElement?: ShadowlistProps<ChatMessage>['renderElement'];
};

const renderChatBubble: ShadowlistProps<ChatMessage>['renderElement'] = ({
  element,
  index,
}) => (
  <ChatBubble
    index={index}
    text={element.text}
    isFromMe={element.isFromMe}
    imageUrl={element.imageUrl}
    imageUrls={element.imageUrls}
  />
);

// An inverted message list (newest at the bottom). Pair with <Chat.Input />.
export const ChatList = forwardRef<ShadowlistCommands, ChatListProps>(
  ({ renderElement, ...props }, ref) => (
    <Shadowlist
      ref={ref}
      inverted
      renderElement={renderElement ?? renderChatBubble}
      {...props}
    />
  )
);
