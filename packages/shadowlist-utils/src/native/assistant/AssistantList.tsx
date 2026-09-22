import { forwardRef, useCallback } from 'react';
import { View, StyleSheet } from 'react-native';
import {
  ShadowList,
  type ElementSizeSpec,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { useLabels } from '../labels';
import { useTheme } from '../theme';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import {
  AssistantReplyMessage,
  type AssistantReplyMessageProps,
} from './AssistantReplyMessage';
import {
  AssistantUserMessage,
  getUserMessageSizeSpec,
} from './AssistantUserMessage';
import { ASSISTANT_END_MARKER_HEIGHT } from './endMarker';
import type { AssistantMessage } from './types';
import type { AssistantStreamStore } from './stream';

type ReplyHandlers = Pick<
  AssistantReplyMessageProps,
  | 'onCopy'
  | 'onCopyCode'
  | 'onShare'
  | 'onRegenerate'
  | 'onRetry'
  | 'onSelectVariant'
  | 'onFeedback'
  | 'onFollowUp'
  | 'onOpenLink'
>;

export type AssistantListProps = Omit<
  ShadowListProps<AssistantMessage>,
  'renderElement'
> &
  ReplyHandlers & {
    renderElement?: ShadowListProps<AssistantMessage>['renderElement'];
    store: AssistantStreamStore;
    // True while a reply streams. Turns off regenerate, retry and edit on every row.
    streaming?: boolean;
    onEdit?: (messageId: string) => void;
    labels?: Partial<AssistantLabels>;
  };

export const AssistantList = forwardRef<ShadowListCommands, AssistantListProps>(
  (
    {
      renderElement,
      store,
      streaming = false,
      onCopy,
      onCopyCode,
      onShare,
      onRegenerate,
      onRetry,
      onSelectVariant,
      onFeedback,
      onFollowUp,
      onOpenLink,
      onEdit,
      labels,
      data,
      ...props
    },
    ref
  ) => {
    const theme = useTheme();
    const l = useLabels(defaultAssistantLabels, labels);

    /*
     * Plain text prompts get an exact spec and the end marker has a fixed height. Replies
     * return null, since Markdown can't be described as one text run and a streaming reply
     * is mounted and measured for real anyway.
     */
    const getElementSizeSpec = useCallback(
      (element: AssistantMessage): ElementSizeSpec | null => {
        switch (element.role) {
          case 'user':
            return getUserMessageSizeSpec(element, theme);
          case 'end':
            return { text: '', fixedHeight: ASSISTANT_END_MARKER_HEIGHT };
          default:
            return null;
        }
      },
      [theme]
    );

    /*
     * Track the newest reply by id, not index. Loading earlier history shifts every index
     * and would rebuild renderElement for rows that did not move. The id only changes when
     * a newer reply arrives, which is when the follow ups should move.
     */
    const latestId = data[data.length - 2]?.id;

    // Changes only when a message is added or removed, never per token.
    const defaultRenderElement = useCallback<
      NonNullable<ShadowListProps<AssistantMessage>['renderElement']>
    >(
      ({ element }) => {
        switch (element.role) {
          case 'user':
            return (
              <AssistantUserMessage
                message={element}
                busy={streaming}
                onCopy={onCopy}
                onEdit={onEdit}
                labels={l}
              />
            );
          case 'assistant':
            return (
              <AssistantReplyMessage
                message={element}
                store={store}
                isLatest={element.id === latestId}
                busy={streaming}
                onCopy={onCopy}
                onCopyCode={onCopyCode}
                onShare={onShare}
                onRegenerate={onRegenerate}
                onRetry={onRetry}
                onSelectVariant={onSelectVariant}
                onFeedback={onFeedback}
                onFollowUp={onFollowUp}
                onOpenLink={onOpenLink}
                labels={l}
              />
            );
          default:
            return <View style={styles.endMarker} />;
        }
      },
      [
        store,
        latestId,
        streaming,
        onCopy,
        onCopyCode,
        onShare,
        onRegenerate,
        onRetry,
        onSelectVariant,
        onFeedback,
        onFollowUp,
        onOpenLink,
        onEdit,
        l,
      ]
    );

    return (
      <ShadowList
        ref={ref}
        data={data}
        inverted
        followAppends
        getElementSizeSpec={getElementSizeSpec}
        renderElement={renderElement ?? defaultRenderElement}
        {...props}
      />
    );
  }
);

const styles = StyleSheet.create({
  endMarker: {
    height: ASSISTANT_END_MARKER_HEIGHT,
  },
});
