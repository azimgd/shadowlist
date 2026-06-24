import { forwardRef } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { ChatBubble, type ChatMessage } from './ChatBubble';

export type ChatListProps = Omit<
  ShadowListProps<ChatMessage>,
  'renderElement'
> & {
  renderElement?: ShadowListProps<ChatMessage>['renderElement'];
};

const renderChatBubble: ShadowListProps<ChatMessage>['renderElement'] = ({
  element,
}) => (
  <ChatBubble
    text={element.text}
    isFromMe={element.isFromMe}
    imageUrl={element.imageUrl}
    imageUrls={element.imageUrls}
    username={element.username}
    avatarColor={element.avatarColor}
    initials={element.initials}
  />
);

/*
 * An inverted message list (newest at the bottom). Renders iMessage-style
 * bubbles from `data`; pair with <Chat.Input /> for a full composer. Wrap in
 * the library's KeyboardView + your own keyboard-avoidance for the full
 * chat experience.
 */
export const ChatList = forwardRef<ShadowListCommands, ChatListProps>(
  ({ renderElement, ...props }, ref) => (
    <ShadowList
      ref={ref}
      inverted
      renderElement={renderElement ?? renderChatBubble}
      {...props}
    />
  )
);
