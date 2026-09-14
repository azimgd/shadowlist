import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { View, StyleSheet, Animated, Clipboard, Share } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import {
  KeyboardView,
  useKeyboardAnimation,
  type ShadowListCommands,
  type ViewToken,
} from 'shadowlist';
import {
  Assistant,
  ListHeader,
  Spinner,
  colors,
  ASSISTANT_END_ID,
  ASSISTANT_END_MARKER,
  ASSISTANT_MODELS,
  ASSISTANT_SUGGESTIONS,
  buildHistory,
  buildReply,
  buildUserMessage,
  createStreamStore,
  emptyTurn,
  pickScript,
  playScript,
  type AssistantAttachment,
  type AssistantComposerHandle,
  type AssistantFeedback,
  type AssistantMessage,
  type AssistantReply,
  type AssistantTurn,
  type StreamHandle,
} from 'shadowlist-utils/native';
import { useListController } from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

/*
 * A streaming AI chat built on the Assistant template: token-by-token Markdown replies
 * with reasoning, tool calls, sources, code copy, stop, regenerate with a version pager,
 * retry after a dropped stream, edit-and-resend, feedback, share, attachments, a model
 * picker, follow-up suggestions, earlier-history loading and a jump-to-latest button.
 *
 * The streaming technique is the point of the screen:
 *
 *   - The list `data` changes twice per reply, when it starts and when it ends. Tokens go
 *     to a store that only the streaming row subscribes to, so the list does no per-token
 *     work -- no data identity change, no key pass, no size-spec rebuild.
 *   - Tokens are coalesced into one flush every STREAM_FLUSH_MS.
 *   - getElementSizeSpec describes prompts exactly and returns null for replies; the row that
 *     streams is always mounted, where a real measurement outranks any prediction.
 *   - Following the stream is native. While the reader is at the bottom, the rows near the
 *     viewport other than the newest are marked non-anchorable, so the core's inverted
 *     bottom pin tracks the newest row as it grows, with no scroll command per flush.
 *   - Leaving the bottom releases following. The core stands its bottom pin down the moment
 *     a user scroll leaves the bottom, so a drag is never fought; the screen then drops the
 *     anchor marking once the end marker has stayed off screen, the text under the reader
 *     holds still while the reply keeps growing below, and the jump button appears.
 *     Returning to the very bottom, or the jump button, resumes following.
 */

// Gap kept between the composer and the keyboard; matches the composer's top padding.
const KEYBOARD_GAP = 8;
// Earlier question/answer pairs loaded per tap of "load earlier".
const HISTORY_PAGE_PAIRS = 3;
// Simulated round trip for loading earlier history.
const HISTORY_LOAD_MS = 700;
/*
 * Rows before the newest one marked non-anchorable while following. Only a row that can be
 * on screen can become the anchor, so a bounded tail is enough, and it keeps the list the
 * core compares on every update short however long the conversation grows.
 */
const FOLLOW_ANCHOR_WINDOW = 30;
/*
 * How long the end marker must stay off screen before following is released. Growth
 * outruns the core's pin by about a flush, so a brief absence is not the reader leaving; a
 * drag keeps it off screen far longer. Scroll deltas are deliberately not used: content
 * shrinking at the bottom clamps the offset down exactly like a small upward drag.
 */
const DISENGAGE_MS = 400;
/*
 * Not following: every row is anchorable, including the end marker. As the last anchorable
 * row it keeps the core from treating a tall reply under the viewport top as "resting at
 * the bottom", so plain anchoring holds the visible text still.
 */
const NOTHING_IGNORED: ReadonlyArray<string> = [];

// A reply with its current variant replaced by `turn`.
function withTurn(reply: AssistantReply, turn: AssistantTurn): AssistantReply {
  return {
    ...reply,
    variants: reply.variants.map((variant, index) =>
      index === reply.variantIndex ? turn : variant
    ),
  };
}

// Applies `update` to the reply `messageId`, leaving every other message untouched.
function updateReply(
  messageId: string,
  update: (reply: AssistantReply) => AssistantReply
) {
  return (prev: AssistantMessage[]) =>
    prev.map((message) =>
      message.id === messageId && message.role === 'assistant'
        ? update(message)
        : message
    );
}

// The reply `messageId`, or undefined when that id is gone or is not a reply.
function findReply(messages: AssistantMessage[], messageId: string) {
  return messages.find(
    (message): message is AssistantReply =>
      message.id === messageId && message.role === 'assistant'
  );
}

/*
 * React Native's built-in Clipboard still works but is deprecated and warns once on first
 * use. It is kept behind this one helper so the example needs no extra native dependency;
 * swap in @react-native-clipboard/clipboard here when it takes one on.
 */
function copyToClipboard(text: string) {
  Clipboard.setString(text);
}

export const AssistantScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const composerRef = useRef<AssistantComposerHandle>(null);
  const insets = useSafeAreaInsets();

  // Live keyboard height (dp); the list and composer translate up by it.
  const { height } = useKeyboardAnimation();

  /*
   * Lift the composer to rest KEYBOARD_GAP above the keyboard once it passes the safe-area
   * inset, so the input keeps the same gap below it as its top padding (not flush).
   */
  const liftTranslateY = useMemo(() => {
    const safe = insets.bottom;
    return height.interpolate({
      inputRange: safe > 0 ? [0, safe, safe + 1] : [0, 1, 2],
      outputRange: [0, -KEYBOARD_GAP, -KEYBOARD_GAP - 1],
    });
  }, [height, insets.bottom]);

  // In-flight turns live here while they stream, outside React state and outside `data`.
  const store = useMemo(() => createStreamStore(), []);
  const streamRef = useRef<StreamHandle | null>(null);
  /*
   * Finished turns stay in the store until their data commit has rendered. A store change
   * renders synchronously and a timer's setData does not, so removing the entry at once
   * would flash the reply's empty committed turn for a frame between the two.
   */
  const settledIdsRef = useRef<string[]>([]);

  const [streaming, setStreaming] = useState(false);
  const [modelIndex, setModelIndex] = useState(0);
  const [thinking, setThinking] = useState(true);
  const [editingId, setEditingId] = useState<string | null>(null);

  /*
   * Whether the view sticks to the newest content. State, because it drives the anchor
   * policy prop; mirrored in a ref for callbacks that must stay referentially stable.
   */
  const [following, setFollowing] = useState(true);
  const followingRef = useRef(true);
  // Whether the end marker is on screen, and a pending release while it stays away.
  const atEndRef = useRef(true);
  const disengageTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  /*
   * A regenerate or retry is streaming into a reply that is not the newest. Emptying that
   * reply can bring the bottom up to the reader, which puts the end marker on screen without
   * the reader going anywhere; resuming following then would chase the bottom and scroll the
   * rewritten text off the top as it arrives. Cleared when that stream finishes.
   */
  const rewritingOlderRef = useRef(false);

  // Read by callbacks that must stay referentially stable for the rows' memoization.
  const modelRef = useRef(ASSISTANT_MODELS[0] ?? '');
  modelRef.current = ASSISTANT_MODELS[modelIndex] ?? '';
  const thinkingRef = useRef(thinking);
  thinkingRef.current = thinking;
  const editingIdRef = useRef(editingId);
  editingIdRef.current = editingId;

  const historyPageRef = useRef(0);
  const suggestionCycleRef = useRef(0);

  // "Load earlier" prepends finished pairs; the core keeps the reader's position.
  const list = useListController<AssistantMessage>({
    onStartReached: () =>
      new Promise<void>((resolve) =>
        setTimeout(() => {
          list.prepend(
            buildHistory(
              HISTORY_PAGE_PAIRS,
              historyPageRef.current * HISTORY_PAGE_PAIRS
            )
          );
          historyPageRef.current += 1;
          resolve();
        }, HISTORY_LOAD_MS)
      ),
  });
  const { setData, append } = list;

  const messagesRef = useRef(list.data);
  messagesRef.current = list.data;

  // Drop finished turns from the store once the commit that carries them has rendered.
  useEffect(() => {
    if (settledIdsRef.current.length === 0) return;
    settledIdsRef.current.forEach((messageId) => store.remove(messageId));
    settledIdsRef.current = [];
  }, [list.data, store]);

  // Stop an in-flight stream and a pending release when leaving the screen.
  useEffect(
    () => () => {
      streamRef.current?.stop();
      if (disengageTimerRef.current) clearTimeout(disengageTimerRef.current);
    },
    []
  );

  // The end marker trails any conversation; an empty one shows the empty state instead.
  const data = useMemo(
    () => (list.data.length > 0 ? [...list.data, ASSISTANT_END_MARKER] : []),
    [list.data]
  );

  /*
   * The anchor policy that makes following native. Following: every row in the tail except
   * the newest is non-anchorable, so the newest is the only anchor candidate and the core
   * pins the true bottom as it grows. Not following: plain anchoring holds the view still.
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
   * Streams a script into the reply `messageId`. The caller has already committed the
   * reply's empty turn; from here every token only touches the store, and onFinish commits
   * the final turn to `data` once. Nothing here scrolls: the anchor policy does.
   */
  const startReply = useCallback(
    (messageId: string, prompt: string, attempt: number) => {
      store.set(messageId, emptyTurn());
      setStreaming(true);
      streamRef.current = playScript(pickScript(prompt, attempt), {
        thinking: thinkingRef.current,
        onUpdate: (turn) => store.set(messageId, turn),
        onFinish: (turn) => {
          streamRef.current = null;
          store.set(messageId, turn);
          settledIdsRef.current.push(messageId);
          setStreaming(false);
          if (rewritingOlderRef.current) {
            rewritingOlderRef.current = false;
            // Nothing grows any more, so a reader already at the bottom can follow again.
            if (atEndRef.current) setFollowingNow(true);
          }
          setData(updateReply(messageId, (reply) => withTurn(reply, turn)));
        },
      });
    },
    [store, setData, setFollowingNow]
  );

  /*
   * `replaceFromId` is the ONLY way to rewind the conversation, and the composer is the
   * only caller that passes it. Reading the edit banner's id in here instead made every
   * other sender (a follow-up chip, a suggestion, the header's next starter) delete
   * everything after whatever prompt the reader happened to be editing at the time.
   */
  const sendPrompt = useCallback(
    (
      text: string,
      attachments: AssistantAttachment[] = [],
      { replaceFromId }: { replaceFromId?: string | null } = {}
    ) => {
      if (streamRef.current) return;

      const prompt = text || 'Describe the attached files';
      const userMessage = buildUserMessage(text, attachments);
      const reply = buildReply(prompt, modelRef.current);

      if (replaceFromId) {
        // Resending an edit rewinds the conversation to that prompt and replaces the rest.
        setData((prev) => {
          const index = prev.findIndex(
            (message) => message.id === replaceFromId
          );
          const kept = index === -1 ? prev : prev.slice(0, index);
          return [...kept, userMessage, reply];
        });
        setEditingId(null);
      } else {
        append([userMessage, reply]);
      }

      // Sending always returns the reader to the bottom and resumes following.
      setFollowingNow(true);
      startReply(reply.id, prompt, 0);
      // The animated flag is unused on both platforms; the core converges on the true end.
      shadowlistRef.current?.scrollToEnd();
    },
    [setData, append, setFollowingNow, startReply]
  );

  // The composer's send, the one that honours the edit banner.
  const handleComposerSend = useCallback(
    (text: string, attachments: AssistantAttachment[]) =>
      sendPrompt(text, attachments, { replaceFromId: editingIdRef.current }),
    [sendPrompt]
  );

  const handleStop = useCallback(() => streamRef.current?.stop(), []);

  /*
   * Regenerating or retrying anything but the NEWEST reply must stop following first.
   * The pin chases the bottom of the conversation, which is somewhere below the reply
   * being rewritten, so the text the reader asked for would scroll off the top as it
   * arrives.
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

  // Adds a new version of the reply and streams into it; the pager shows every version.
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
      startReply(messageId, reply.prompt, reply.variants.length);
    },
    [setData, startReply, releaseFollowingUnlessNewest]
  );

  // Replays the prompt into the failed version itself rather than adding a new one.
  const handleRetry = useCallback(
    (messageId: string) => {
      if (streamRef.current) return;
      const reply = findReply(messagesRef.current, messageId);
      if (!reply) return;

      releaseFollowingUnlessNewest(messageId);

      setData(
        updateReply(messageId, (current) => withTurn(current, emptyTurn()))
      );
      startReply(messageId, reply.prompt, reply.variants.length);
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
    (messageId: string, feedback: AssistantFeedback) =>
      setData(updateReply(messageId, (reply) => ({ ...reply, feedback }))),
    [setData]
  );

  const handleShare = useCallback((text: string) => {
    Share.share({ message: text }).catch(() => {});
  }, []);

  // Loads the prompt back into the composer; sending it replaces everything from it on.
  const handleEdit = useCallback((messageId: string) => {
    if (streamRef.current) return;
    const prompt = messagesRef.current.find(
      (message) => message.id === messageId
    );
    if (!prompt || prompt.role !== 'user') return;
    composerRef.current?.setDraft(prompt.text, prompt.attachments);
    setEditingId(messageId);
  }, []);

  const handleCancelEdit = useCallback(() => {
    setEditingId(null);
    // clearDraft, not setDraft(''): cancelling must not raise the keyboard over the thread.
    composerRef.current?.clearDraft();
  }, []);

  /*
   * A suggestion or a follow-up chip is a new question, not the edit in the banner. Drop
   * the pending edit first so the banner and its stale draft do not outlive it.
   */
  const handleSelectPrompt = useCallback(
    (prompt: string) => {
      handleCancelEdit();
      sendPrompt(prompt);
    },
    [handleCancelEdit, sendPrompt]
  );

  const handleCycleModel = useCallback(
    () => setModelIndex((index) => (index + 1) % ASSISTANT_MODELS.length),
    []
  );

  const handleToggleThinking = useCallback(
    () => setThinking((current) => !current),
    []
  );

  /*
   * The end marker is 1pt at the very end of the content, so it is on screen only at the true
   * bottom. Arriving there resumes following at once. Leaving releases following only if it
   * stays away for DISENGAGE_MS: growth outruns the pin for a flush, and content shrinking at
   * the bottom (a code fence closing, a preview line collapsing) clamps the view to the new
   * bottom without the marker ever leaving, so neither is mistaken for the reader.
   */
  const handleViewableItemsChanged = useCallback(
    ({ viewableItems }: { viewableItems: ViewToken<AssistantMessage>[] }) => {
      if (messagesRef.current.length === 0) return;
      const endOnScreen = viewableItems.some(
        (token) => token.item.role === 'end'
      );
      atEndRef.current = endOnScreen;

      if (endOnScreen) {
        // Not while an older reply is being rewritten; see rewritingOlderRef.
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
    // An explicit jump overrides the rewrite hold: the reader asked for the bottom.
    rewritingOlderRef.current = false;
    setFollowingNow(true);
    shadowlistRef.current?.scrollToEnd();
  }, [setFollowingNow]);

  // Nav-bar controls, as on every screen: load earlier / ask the next starter / jump.
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
          <ListHeader title="Assistant" subtitle="Streaming AI chat" />
          {list.loadingOlder ? <Spinner size={16} /> : null}
        </View>
      ) : null,
    [hasMessages, list.loadingOlder]
  );

  /*
   * Rendered over the list rather than as ListEmptyComponent: the core's total size is
   * header + rows + footer and never includes the empty template, so with no messages the
   * content container is 0pt tall and Android clips the template out of view.
   */
  const empty = useMemo(
    () => (
      <Assistant.Empty
        suggestions={ASSISTANT_SUGGESTIONS}
        onSelect={handleSelectPrompt}
      />
    ),
    [handleSelectPrompt]
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
             * Following is already the "is the reader at the bottom" answer -- arriving at
             * the end marker sets it, leaving for DISENGAGE_MS clears it. A second atEnd
             * state alongside it only adds a render and a window where the two disagree.
             */
            visible={hasMessages && !following}
            onPress={handleScrollToLatest}
          />
        </KeyboardView>
        <Assistant.Composer
          ref={composerRef}
          streaming={streaming}
          model={ASSISTANT_MODELS[modelIndex] ?? ''}
          onCycleModel={handleCycleModel}
          thinking={thinking}
          onToggleThinking={handleToggleThinking}
          editing={editingId !== null}
          onCancelEdit={handleCancelEdit}
          onSend={handleComposerSend}
          onStop={handleStop}
        />
      </Animated.View>
    </View>
  );
};

const styles = StyleSheet.create({
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
});
