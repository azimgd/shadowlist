import { forwardRef, useCallback, useMemo } from 'react';
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

    /*
     * Rows read busy and the newest reply from context, down in the buttons and follow ups
     * that use them. In renderElement they gave it a new identity when a reply started and
     * again when it ended, which rebuilt every mounted row both times.
     */
    const rowState = useMemo<AssistantRowState>(
      () => ({ busy: streaming, latestId }),
      [streaming, latestId]
    );

    // Changes only when a handler or the labels change, never per reply or token.
    const defaultRenderElement = useCallback<
      NonNullable<ShadowListProps<AssistantMessage>['renderElement']>
    >(
      ({ element }) => {
        switch (element.role) {
          case 'user':
            return (
              <AssistantUserMessage
                message={element}
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
          getElementSizeSpec={getElementSizeSpec}
          renderElement={renderElement ?? defaultRenderElement}
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
