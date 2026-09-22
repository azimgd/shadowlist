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
  ShadowListProps<ChatMessage>['renderElement']
>;

export type ChatListProps = Omit<
  ShadowListProps<ChatMessage>,
  'renderElement'
> & {
  renderElement?: RenderChatMessage;
  // Passed to the default bubble. Keep them stable, or every mounted bubble re-renders.
  onRetryMessage?: (message: ChatMessage) => void;
  onLongPressMessage?: (message: ChatMessage) => void;
  labels?: Partial<ChatLabels>;
};

export const ChatList = forwardRef<ShadowListCommands, ChatListProps>(
  (
    {
      renderElement,
      getElementSizeSpec,
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
      ({ element }) => (
        <ChatBubble
          message={element}
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
        renderElement={renderElement ?? renderBubble}
        // The default spec only fits the default bubble. A custom renderer brings its own.
        getElementSizeSpec={
          getElementSizeSpec ??
          (renderElement === undefined ? getDefaultSizeSpec : undefined)
        }
        {...props}
      />
    );
  }
);
