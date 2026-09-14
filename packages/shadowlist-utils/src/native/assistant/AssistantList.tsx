import { forwardRef, useCallback } from 'react';
import { View, StyleSheet } from 'react-native';
import {
  ShadowList,
  type ElementSizeSpec,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import {
  AssistantReplyMessage,
  type AssistantReplyMessageProps,
} from './AssistantReplyMessage';
import {
  AssistantUserMessage,
  getUserMessageSizeSpec,
} from './AssistantUserMessage';
import type { AssistantMessage } from './data';
import type { AssistantStreamStore } from './stream';

// The end marker's height: invisible, but a real row, so "it is viewable" means "the bottom is on screen".
const ASSISTANT_END_MARKER_HEIGHT = 1;

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
>;

export type AssistantListProps = Omit<
  ShadowListProps<AssistantMessage>,
  'renderElement'
> &
  ReplyHandlers & {
    renderElement?: ShadowListProps<AssistantMessage>['renderElement'];
    // Where streaming replies read their in-flight turn from.
    store: AssistantStreamStore;
    /*
     * A reply is streaming somewhere in the conversation. Forwarded to the rows so the
     * actions that would start a second one (edit, regenerate, retry) are disabled.
     */
    streaming?: boolean;
    onEdit?: (messageId: string) => void;
  };

/*
 * Ahead-of-time sizes (ShadowListProps.getElementSizeSpec). Plain-text prompts are described
 * exactly and the end marker has a fixed height. Replies return null: Markdown with code
 * and tables is not one text run, and the reply that is streaming is mounted by
 * definition, where a real measurement outranks any prediction -- describing it would
 * only cost work that is thrown away.
 */
function getAssistantElementSizeSpec(
  element: AssistantMessage
): ElementSizeSpec | null {
  switch (element.role) {
    case 'user':
      return getUserMessageSizeSpec(element);
    case 'end':
      return { text: '', fixedHeight: ASSISTANT_END_MARKER_HEIGHT };
    default:
      return null;
  }
}

/*
 * An inverted assistant conversation (newest at the bottom): prompts, streaming Markdown
 * replies and a trailing end marker. Pair with <Assistant.Composer /> and
 * <Assistant.ScrollButton />, and wrap in the library's KeyboardView plus your own
 * keyboard lift, as on the Chat template.
 */
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
      onEdit,
      data,
      ...props
    },
    ref
  ) => {
    /*
     * The newest reply, by IDENTITY rather than by index. An index is a function of the
     * list length, so prepending a page of earlier history renames the newest row and
     * rebuilds renderElement for rows that did not move at all; the id changes only when a
     * genuinely newer reply is appended, which is exactly when the follow-ups must move.
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
        onEdit,
      ]
    );

    return (
      <ShadowList
        ref={ref}
        data={data}
        inverted
        getElementSizeSpec={getAssistantElementSizeSpec}
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
