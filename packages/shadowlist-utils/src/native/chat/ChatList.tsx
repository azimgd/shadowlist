import { forwardRef, useCallback } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { useLabels } from '../labels';
import { useTheme } from '../theme';
import { ChatBubble } from './ChatBubble';
import { defaultChatLabels, type ChatLabels } from './labels';
import { getChatMessageSizeSpec } from './sizeSpec';
import type { ChatMessage } from './types';

type RenderChatMessage = NonNullable<
  ShadowListProps<ChatMessage>['renderItem']
>;

export type ChatListProps = Omit<ShadowListProps<ChatMessage>, 'renderItem'> & {
  renderItem?: RenderChatMessage;
  onRetryMessage?: (message: ChatMessage) => void;
  onLongPressMessage?: (message: ChatMessage) => void;
  labels?: Partial<ChatLabels>;
};

export const ChatList = forwardRef<ShadowListCommands, ChatListProps>(
  (
    {
      renderItem,
      getItemSizeSpec,
      onRetryMessage,
      onLongPressMessage,
      labels,
      ...props
    },
    ref
  ) => {
    const theme = useTheme();
    const bubbleLabels = useLabels(defaultChatLabels, labels);
    const renderBubble = useCallback<RenderChatMessage>(
      ({ item }) => (
        <ChatBubble
          message={item}
          onRetry={onRetryMessage}
          onLongPress={onLongPressMessage}
          labels={bubbleLabels}
        />
      ),
      [bubbleLabels, onRetryMessage, onLongPressMessage]
    );
    const getDefaultSizeSpec = useCallback(
      (message: ChatMessage) => getChatMessageSizeSpec(message, theme),
      [theme]
    );
    return (
      <ShadowList
        ref={ref}
        inverted
        renderItem={renderItem ?? renderBubble}
        // The default spec only fits the default bubble. A custom renderer brings its own.
        getItemSizeSpec={
          getItemSizeSpec ??
          (renderItem === undefined ? getDefaultSizeSpec : undefined)
        }
        {...props}
      />
    );
  }
);
