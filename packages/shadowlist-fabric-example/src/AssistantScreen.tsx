import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { View, StyleSheet, Animated, Clipboard, Share } from 'react-native';
import {
  KeyboardView,
  type ShadowListCommands,
  type ViewToken,
} from 'shadowlist';
import { useListController } from 'shadowlist-utils';
import {
  Assistant,
  ListHeader,
  Spinner,
  ASSISTANT_END_ID,
  ASSISTANT_END_MARKER,
  createStreamStore,
  createTurnWriter,
  emptyTurn,
  type AssistantAttachment,
  type AssistantComposerHandle,
  type AssistantFeedback,
  type AssistantLabels,
  type AssistantMessage,
  type AssistantReply,
  type AssistantSuggestion,
  type AssistantTurn,
  createStyles,
  useKeyboardLift,
} from 'shadowlist-utils/native';
import {
  ASSISTANT_MODELS,
  ASSISTANT_SUGGESTIONS,
  ATTACHMENTS_ONLY_PROMPT,
  buildAttachment,
  buildReply,
  buildUserMessage,
  pickScript,
  playScript,
  type ScriptPlayback,
} from './fixtures/assistant';
import { useHeaderActions } from './HeaderActions';
import {
  useFetchAssistantHistory,
  useSendAssistantFeedback,
} from './queries/assistant';

/*
 * A streaming AI chat built on the Assistant template, with Markdown replies, reasoning, tool calls,
 * sources, stop, regenerate, retry, edit and resend, attachments, suggestions, earlier history and
 * a jump to latest button.
 *
 * How streaming stays cheap:
 * The list data changes only when a reply starts and ends. Tokens go to a store that only the
 * streaming row reads, so the list does no work per token. Tokens are batched into one flush
 * every STREAM_FLUSH_MS.
 * getElementSizeSpec sizes prompts exactly and returns null for replies. The streaming row is
 * always mounted, so its real size wins over any guess.
 * Following the stream is native. At the bottom, the rows near the newest one are marked as not
 * anchorable, so the core keeps the newest row pinned as it grows, with no scroll per flush.
 * A user scroll away from the bottom stops the pin at once, so a drag is never fought. Once the
 * end marker stays off screen, the anchor marks are dropped, the text holds still while the reply
 * grows below, and the jump button shows. Coming back to the bottom resumes following.
 */

const KEYBOARD_GAP = 8;
/*
 * How many rows before the newest get marked as not anchorable while following. Only rows that
 * can be on screen matter, so a short tail is enough and keeps the list the core checks small.
 */
const FOLLOW_ANCHOR_WINDOW = 30;
/*
 * How long the end marker must stay off screen before following stops. Growth runs about a
 * flush ahead of the pin, so a short gap is not the reader leaving. Scroll deltas are not used,
 * because content shrinking at the bottom moves the offset just like a small upward drag.
 */
const DISENGAGE_MS = 400;
/*
 * Not following: every row can anchor, the end marker too. As the last one it stops the core
 * from treating a tall reply as resting at the bottom, so the visible text holds still.
 */
const NOTHING_IGNORED: ReadonlyArray<string> = [];

const EMPTY_LABELS: Partial<AssistantLabels> = {
  emptyTitle: 'Where are we flying?',
  emptySubtitle: 'Plan flights, weather and stays.',
};

function withTurn(reply: AssistantReply, turn: AssistantTurn): AssistantReply {
  return {
    ...reply,
    variants: reply.variants.map((variant, index) =>
      index === reply.variantIndex ? turn : variant
    ),
  };
}

function updateReply(
  messageId: string,
  update: (reply: AssistantReply) => AssistantReply
) {
  return (previous: AssistantMessage[]) =>
    previous.map((message) =>
      message.id === messageId && message.role === 'assistant'
        ? update(message)
        : message
    );
}

function findReply(messages: AssistantMessage[], messageId: string) {
  return messages.find(
    (message): message is AssistantReply =>
      message.id === messageId && message.role === 'assistant'
  );
}

/*
 * The prompt a reply answers is the user message right before it.
 */
function findPromptFor(messages: AssistantMessage[], replyId: string) {
  const index = messages.findIndex((message) => message.id === replyId);
  const prompt = messages[index - 1];
  if (!prompt || prompt.role !== 'user') return ATTACHMENTS_ONLY_PROMPT;
  return prompt.text || ATTACHMENTS_ONLY_PROMPT;
}

function copyToClipboard(text: string) {
  Clipboard.setString(text);
}

export const AssistantScreen = () => {
  const styles = useStyles();
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const composerRef = useRef<AssistantComposerHandle>(null);

  const liftTranslateY = useKeyboardLift({ gap: KEYBOARD_GAP });

  const store = useMemo(() => createStreamStore(), []);
  const streamRef = useRef<ScriptPlayback | null>(null);
  /*
   * Keep finished turns in the store until their data commit renders. A store change renders at
   * once but a timer's setData does not, so removing early would flash an empty reply for a frame.
   */
  const settledIdsRef = useRef<string[]>([]);

  const [streaming, setStreaming] = useState(false);
  const [modelIndex, setModelIndex] = useState(0);
  const [thinking, setThinking] = useState(true);
  const [editingId, setEditingId] = useState<string | null>(null);
  const [attachments, setAttachments] = useState<AssistantAttachment[]>([]);
  const attachCountRef = useRef(0);

  const [following, setFollowing] = useState(true);
  const followingRef = useRef(true);
  const atEndRef = useRef(true);
  const disengageTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  /*
   * Set while a regenerate or retry streams into a reply that is not the newest. Emptying that
   * reply can bring the end marker on screen by itself, and following then would scroll the new
   * text off the top. Cleared when that stream finishes.
   */
  const rewritingOlderRef = useRef(false);

  const modelRef = useRef(ASSISTANT_MODELS[0] ?? '');
  modelRef.current = ASSISTANT_MODELS[modelIndex] ?? '';
  const thinkingRef = useRef(thinking);
  thinkingRef.current = thinking;
  const editingIdRef = useRef(editingId);
  editingIdRef.current = editingId;

  const historyPageRef = useRef(0);
  const suggestionCycleRef = useRef(0);

  const fetchHistoryPage = useFetchAssistantHistory();
  const { mutate: sendFeedback } = useSendAssistantFeedback();

  const list = useListController<AssistantMessage>({
    onStartReached: async () => {
      const page = historyPageRef.current;
      const history = await fetchHistoryPage(page);
      list.prepend(history);
      historyPageRef.current = page + 1;
    },
  });
  const { setData, append } = list;

  const messagesRef = useRef(list.data);
  messagesRef.current = list.data;

  useEffect(() => {
    if (settledIdsRef.current.length === 0) return;
    settledIdsRef.current.forEach((messageId) => store.remove(messageId));
    settledIdsRef.current = [];
  }, [list.data, store]);

  useEffect(
    () => () => {
      streamRef.current?.stop();
      if (disengageTimerRef.current) clearTimeout(disengageTimerRef.current);
    },
    []
  );

  const data = useMemo(
    () => (list.data.length > 0 ? [...list.data, ASSISTANT_END_MARKER] : []),
    [list.data]
  );

  /*
   * The anchor policy that makes following native. While following, only the newest row in the
   * tail can anchor, so the core pins the bottom as it grows. Otherwise plain anchoring holds the
   * view still.
   */
  const nonAnchorKeys = useMemo(() => {
    if (!following || list.data.length === 0) return NOTHING_IGNORED;
    const start = Math.max(0, list.data.length - 1 - FOLLOW_ANCHOR_WINDOW);
    return [
      ...list.data.slice(start, -1).map((message) => message.id),
      ASSISTANT_END_ID,
    ];
  }, [following, list.data]);

  const setFollowingNow = useCallback((next: boolean) => {
    if (disengageTimerRef.current) {
      clearTimeout(disengageTimerRef.current);
      disengageTimerRef.current = null;
    }
    if (followingRef.current === next) return;
    followingRef.current = next;
    setFollowing(next);
  }, []);

  /*
   * Streams a script into the reply messageId. The caller has already committed the empty turn.
   * Each token only touches the store, and onFinish commits the final turn once. Nothing here
   * scrolls, the anchor policy does that.
   */
  const startReply = useCallback(
    (messageId: string, prompt: string, attempt: number) => {
      const writer = createTurnWriter({
        store,
        messageId,
        onFinish: (turn) => {
          streamRef.current = null;
          settledIdsRef.current.push(messageId);
          setStreaming(false);
          if (rewritingOlderRef.current) {
            rewritingOlderRef.current = false;
            if (atEndRef.current) setFollowingNow(true);
          }
          setData(updateReply(messageId, (reply) => withTurn(reply, turn)));
        },
      });
      setStreaming(true);
      streamRef.current = playScript(pickScript(prompt, attempt), writer, {
        thinking: thinkingRef.current,
      });
    },
    [store, setData, setFollowingNow]
  );

  /*
   * Only replaceFromId rewinds the conversation, and only the composer passes it. Reading the edit
   * banner's id in here made every other sender, like a follow-up chip, delete everything after
   * the prompt being edited.
   */
  const sendPrompt = useCallback(
    (
      text: string,
      promptAttachments: AssistantAttachment[] = [],
      { replaceFromId }: { replaceFromId?: string | null } = {}
    ) => {
      if (streamRef.current) return;

      const prompt = text || ATTACHMENTS_ONLY_PROMPT;
      const userMessage = buildUserMessage(text, promptAttachments);
      const reply = buildReply(modelRef.current);

      if (replaceFromId) {
        setData((previous) => {
          const index = previous.findIndex(
            (message) => message.id === replaceFromId
          );
          const kept = index === -1 ? previous : previous.slice(0, index);
          return [...kept, userMessage, reply];
        });
        setEditingId(null);
      } else {
        append([userMessage, reply]);
      }

      setFollowingNow(true);
      startReply(reply.id, prompt, 0);
      shadowlistRef.current?.scrollToEnd();
    },
    [setData, append, setFollowingNow, startReply]
  );

  const handleComposerSend = useCallback(
    (text: string, sent: readonly AssistantAttachment[]) => {
      setAttachments([]);
      sendPrompt(text, [...sent], { replaceFromId: editingIdRef.current });
    },
    [sendPrompt]
  );

  const handleAttachPress = useCallback(
    () =>
      setAttachments((previous) => [
        ...previous,
        buildAttachment(attachCountRef.current++),
      ]),
    []
  );

  const handleRemoveAttachment = useCallback(
    (attachmentId: string) =>
      setAttachments((previous) =>
        previous.filter((attachment) => attachment.id !== attachmentId)
      ),
    []
  );

  const handleStop = useCallback(() => streamRef.current?.stop(), []);

  /*
   * Regenerating or retrying any reply but the newest must stop following first. The pin chases
   * the bottom, so the rewritten text would scroll off the top as it arrives.
   */
  const releaseFollowingUnlessNewest = useCallback(
    (messageId: string) => {
      const messages = messagesRef.current;
      if (messages[messages.length - 1]?.id !== messageId) {
        rewritingOlderRef.current = true;
        setFollowingNow(false);
      }
    },
    [setFollowingNow]
  );

  const handleRegenerate = useCallback(
    (messageId: string) => {
      if (streamRef.current) return;
      const reply = findReply(messagesRef.current, messageId);
      if (!reply) return;

      releaseFollowingUnlessNewest(messageId);

      setData(
        updateReply(messageId, (current) => ({
          ...current,
          variants: [...current.variants, emptyTurn()],
          variantIndex: current.variants.length,
        }))
      );
      startReply(
        messageId,
        findPromptFor(messagesRef.current, messageId),
        reply.variants.length
      );
    },
    [setData, startReply, releaseFollowingUnlessNewest]
  );

  const handleRetry = useCallback(
    (messageId: string) => {
      if (streamRef.current) return;
      const reply = findReply(messagesRef.current, messageId);
      if (!reply) return;

      releaseFollowingUnlessNewest(messageId);

      setData(
        updateReply(messageId, (current) => withTurn(current, emptyTurn()))
      );
      startReply(
        messageId,
        findPromptFor(messagesRef.current, messageId),
        reply.variants.length
      );
    },
    [setData, startReply, releaseFollowingUnlessNewest]
  );

  const handleSelectVariant = useCallback(
    (messageId: string, variantIndex: number) =>
      setData(
        updateReply(messageId, (reply) => ({
          ...reply,
          variantIndex: Math.max(
            0,
            Math.min(variantIndex, reply.variants.length - 1)
          ),
        }))
      ),
    [setData]
  );

  const handleFeedback = useCallback(
    (messageId: string, feedback: AssistantFeedback | undefined) => {
      const previous = findReply(messagesRef.current, messageId)?.feedback;
      setData(updateReply(messageId, (reply) => ({ ...reply, feedback })));
      sendFeedback(
        { messageId, feedback },
        {
          onError: () =>
            setData(
              updateReply(messageId, (reply) => ({
                ...reply,
                feedback: previous,
              }))
            ),
        }
      );
    },
    [setData, sendFeedback]
  );

  const handleShare = useCallback((text: string) => {
    Share.share({ message: text }).catch(() => {});
  }, []);

  const handleEdit = useCallback((messageId: string) => {
    if (streamRef.current) return;
    const prompt = messagesRef.current.find(
      (message) => message.id === messageId
    );
    if (!prompt || prompt.role !== 'user') return;
    composerRef.current?.setDraft(prompt.text);
    setAttachments([...(prompt.attachments ?? [])]);
    setEditingId(messageId);
  }, []);

  const handleCancelEdit = useCallback(() => {
    setEditingId(null);
    // Use clearDraft rather than an empty setDraft so cancelling does not raise the keyboard.
    composerRef.current?.clearDraft();
    setAttachments([]);
  }, []);

  /*
   * A suggestion or follow-up chip is a new question, not the edit in the banner.
   * Drop the pending edit first so the banner and its old draft go away.
   */
  const handleSelectPrompt = useCallback(
    (prompt: string) => {
      handleCancelEdit();
      sendPrompt(prompt);
    },
    [handleCancelEdit, sendPrompt]
  );

  const handleSelectSuggestion = useCallback(
    (suggestion: AssistantSuggestion) => handleSelectPrompt(suggestion.prompt),
    [handleSelectPrompt]
  );

  const handleCycleModel = useCallback(
    () => setModelIndex((index) => (index + 1) % ASSISTANT_MODELS.length),
    []
  );

  /*
   * The end marker is 1pt at the very end, so it is on screen only at the true bottom. Reaching
   * it resumes following at once. Leaving stops following only after DISENGAGE_MS, since growth
   * runs a flush ahead of the pin, and neither that nor content shrinking at the bottom should
   * count as the reader leaving.
   */
  const handleViewableItemsChanged = useCallback(
    ({ viewableItems }: { viewableItems: ViewToken<AssistantMessage>[] }) => {
      if (messagesRef.current.length === 0) return;
      const endOnScreen = viewableItems.some(
        (token) => token.item.role === 'end'
      );
      atEndRef.current = endOnScreen;

      if (endOnScreen) {
        // Not while an older reply is being rewritten, see rewritingOlderRef.
        if (!rewritingOlderRef.current) setFollowingNow(true);
        return;
      }
      if (!followingRef.current || disengageTimerRef.current) return;
      disengageTimerRef.current = setTimeout(() => {
        disengageTimerRef.current = null;
        if (!atEndRef.current) setFollowingNow(false);
      }, DISENGAGE_MS);
    },
    [setFollowingNow]
  );

  const handleScrollToLatest = useCallback(() => {
    // A jump overrides the rewrite hold because the reader asked for the bottom.
    rewritingOlderRef.current = false;
    setFollowingNow(true);
    shadowlistRef.current?.scrollToEnd();
  }, [setFollowingNow]);

  useHeaderActions({
    onPrepend: list.handleStartReached,
    onAppend: () => {
      const suggestion =
        ASSISTANT_SUGGESTIONS[
          suggestionCycleRef.current++ % ASSISTANT_SUGGESTIONS.length
        ];
      if (suggestion) handleSelectPrompt(suggestion.prompt);
    },
    onScrollToRandom: () =>
      shadowlistRef.current?.scrollToIndex(
        Math.floor(Math.random() * data.length)
      ),
  });

  const hasMessages = list.data.length > 0;

  const header = useMemo(
    () =>
      hasMessages ? (
        <View>
          <ListHeader
            title="Skyfy Assistant"
            subtitle="Your trip-planning copilot"
          />
          {list.loadingOlder ? <Spinner size={16} /> : null}
        </View>
      ) : null,
    [hasMessages, list.loadingOlder]
  );

  /*
   * Drawn over the list instead of as ListEmptyComponent. The core's total size never includes
   * the empty template, so with no messages the content is 0pt tall and Android clips it.
   */
  const empty = useMemo(
    () => (
      <Assistant.Empty
        suggestions={ASSISTANT_SUGGESTIONS}
        onSelectSuggestion={handleSelectSuggestion}
        labels={EMPTY_LABELS}
      />
    ),
    [handleSelectSuggestion]
  );

  return (
    <View style={styles.container}>
      <Animated.View
        style={[styles.lifted, { transform: [{ translateY: liftTranslateY }] }]}
      >
        <KeyboardView style={styles.list}>
          <Assistant.List
            data={data}
            ref={shadowlistRef}
            store={store}
            style={styles.list}
            streaming={streaming}
            nonAnchorKeys={nonAnchorKeys}
            ListHeaderComponent={header}
            onStartReached={list.handleStartReached}
            onViewableItemsChanged={handleViewableItemsChanged}
            onCopy={copyToClipboard}
            onCopyCode={copyToClipboard}
            onShare={handleShare}
            onRegenerate={handleRegenerate}
            onRetry={handleRetry}
            onSelectVariant={handleSelectVariant}
            onFeedback={handleFeedback}
            onFollowUp={handleSelectPrompt}
            onEdit={handleEdit}
          />
          {hasMessages ? null : (
            <View style={styles.emptyOverlay}>{empty}</View>
          )}
          <Assistant.ScrollButton
            /*
             * Following already says whether the reader is at the bottom. A second atEnd state would
             * only add a render and a moment where the two disagree.
             */
            visible={hasMessages && !following}
            onPress={handleScrollToLatest}
          />
        </KeyboardView>
        <Assistant.Composer
          ref={composerRef}
          streaming={streaming}
          model={ASSISTANT_MODELS[modelIndex] ?? ''}
          onPressModel={handleCycleModel}
          thinking={thinking}
          onThinkingChange={setThinking}
          attachments={attachments}
          onPressAttach={handleAttachPress}
          onRemoveAttachment={handleRemoveAttachment}
          editing={editingId !== null}
          onCancelEdit={handleCancelEdit}
          onSend={handleComposerSend}
          onStop={handleStop}
        />
      </Animated.View>
    </View>
  );
};

const useStyles = createStyles(({ colors }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      backgroundColor: colors.background,
      overflow: 'hidden',
    },
    lifted: {
      flex: 1,
      backgroundColor: colors.background,
    },
    list: {
      flex: 1,
      backgroundColor: colors.background,
    },
    emptyOverlay: {
      position: 'absolute',
      top: 0,
      right: 0,
      bottom: 0,
      left: 0,
      backgroundColor: colors.background,
    },
  })
);
