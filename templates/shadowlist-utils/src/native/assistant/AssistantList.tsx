import { forwardRef, useCallback, useMemo } from 'react';
import { View, StyleSheet } from 'react-native';
import {
  ShadowList,
  type ItemSizeSpec,
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
import { AssistantRowStateContext, type AssistantRowState } from './rowState';

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
  'renderItem'
> &
  ReplyHandlers & {
    renderItem?: ShadowListProps<AssistantMessage>['renderItem'];
    store: AssistantStreamStore;
    streaming?: boolean;
    onEdit?: (messageId: string) => void;
    labels?: Partial<AssistantLabels>;
  };

export const AssistantList = forwardRef<ShadowListCommands, AssistantListProps>(
  (
    {
      renderItem,
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
    const getItemSizeSpec = useCallback(
      (item: AssistantMessage): ItemSizeSpec | null => {
        switch (item.role) {
          case 'user':
            return getUserMessageSizeSpec(item, theme);
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
     * and would rebuild renderItem for rows that did not move. The id only changes when
     * a newer reply arrives, which is when the follow ups should move.
     */
    const latestId = data[data.length - 2]?.id;

    /*
     * Rows read busy and the newest reply from context, down in the buttons and follow ups
     * that use them. In renderItem they would give it a new identity when a reply starts
     * and again when it ends, rebuilding every mounted row both times.
     */
    const rowState = useMemo<AssistantRowState>(
      () => ({ busy: streaming, latestId }),
      [streaming, latestId]
    );

    // Changes only when a handler or the labels change, never per reply or token.
    const defaultRenderItem = useCallback<
      NonNullable<ShadowListProps<AssistantMessage>['renderItem']>
    >(
      ({ item }) => {
        switch (item.role) {
          case 'user':
            return (
              <AssistantUserMessage
                message={item}
                onCopy={onCopy}
                onEdit={onEdit}
                labels={l}
              />
            );
          case 'assistant':
            return (
              <AssistantReplyMessage
                message={item}
                store={store}
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
      <AssistantRowStateContext.Provider value={rowState}>
        <ShadowList
          ref={ref}
          data={data}
          inverted
          followAppends
          getItemSizeSpec={getItemSizeSpec}
          renderItem={renderItem ?? defaultRenderItem}
          {...props}
        />
      </AssistantRowStateContext.Provider>
    );
  }
);

const styles = StyleSheet.create({
  endMarker: {
    height: ASSISTANT_END_MARKER_HEIGHT,
  },
});
