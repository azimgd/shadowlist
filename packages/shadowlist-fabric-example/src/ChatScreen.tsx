import { useCallback, useEffect, useMemo, useRef } from 'react';
import { View, StyleSheet, Animated } from 'react-native';
import { KeyboardView, type ShadowListCommands } from 'shadowlist';
import { useInfiniteListProps } from 'shadowlist-utils';
import {
  Chat,
  ListHeader,
  ListFooter,
  Spinner,
  getChatMessageSizeSpec,
  createStyles,
  useKeyboardLift,
  useTheme,
  type ChatMessage,
} from 'shadowlist-utils/native';
import { useItemOrdinals } from './itemOrdinals';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { createOutgoingMessage, simulateIncomingMessages } from './api/chat';
import {
  useChatMessagesQuery,
  useIncomingChatMessages,
  useSendChatMessage,
} from './queries/chat';

const KEYBOARD_GAP = 8;
const INCOMING_COUNT = 10;
const INPUT_LABELS = { placeholder: 'Message your crew' };

export const ChatScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const theme = useTheme();
  const styles = useStyles();
  const liftTranslateY = useKeyboardLift({ gap: KEYBOARD_GAP });

  const messages = useChatMessagesQuery();
  const list = useInfiniteListProps(messages);
  const { mutate: sendMessage } = useSendChatMessage();
  useIncomingChatMessages();

  /*
   * Incoming messages keep the reader where they are (the list does not followAppends); the
   * reader's own message is brought into view once it is in the list.
   */
  const sentIdRef = useRef<string | null>(null);
  const handleSendMessage = useCallback(
    (text: string) => {
      const message = createOutgoingMessage(text);
      sentIdRef.current = message.id;
      sendMessage(message);
    },
    [sendMessage]
  );
  const lastMessage = list.data[list.data.length - 1];
  const lastId = lastMessage?.id;
  useEffect(() => {
    if (lastId !== undefined && lastId === sentIdRef.current) {
      sentIdRef.current = null;
      shadowlistRef.current?.scrollToEnd();
    }
  }, [lastId]);

  /*
   * A failed send grows its row by the retry line. The list keeps the visible area where it
   * is, so on the newest message that line would land under the composer, out of sight.
   */
  const lastFailed =
    lastMessage?.isOwn === true && lastMessage.status === 'failed';
  useEffect(() => {
    if (lastFailed) shadowlistRef.current?.scrollToEnd();
  }, [lastFailed, lastId]);

  /*
   * The position of each message in the thread, under its bubble. Numbers are assigned once
   * per message id (see useItemOrdinals), so loading older history labels only the new page
   * and leaves every mounted bubble alone -- both the renderer and the size spec below stay
   * referentially stable for the same reason.
   */
  const ordinals = useItemOrdinals(list.data);
  const { labelOf } = ordinals;

  // The same id goes out again; the bubble flips back to 'sending' in place.
  const handleRetry = useCallback(
    (message: ChatMessage) => sendMessage(message),
    [sendMessage]
  );

  const renderBubble = useCallback(
    ({ element }: { element: ChatMessage }) => (
      <Chat.Bubble
        message={element}
        caption={labelOf(element.id)}
        onRetry={handleRetry}
      />
    ),
    [labelOf, handleRetry]
  );

  // The caption is a line of its own, so the predicted height has to include it (the spec
  // adds a failed message's status line by itself).
  const getSizeSpec = useCallback(
    (message: ChatMessage) =>
      getChatMessageSizeSpec(message, theme, { caption: true }),
    [theme]
  );

  useHeaderActions({
    onPrepend: list.onStartReached,
    onAppend: () => simulateIncomingMessages(INCOMING_COUNT),
    onScrollToRandom: () =>
      shadowlistRef.current?.scrollToIndex(
        Math.floor(Math.random() * list.data.length)
      ),
  });

  const { isFetchingPreviousPage, hasPreviousPage } = messages;
  const header = useMemo(
    () => (
      <View>
        <ListHeader
          title="Lisbon Crew"
          subtitle="8 travellers · departs Oct 12"
        />
        {isFetchingPreviousPage ? <Spinner size={16} /> : null}
      </View>
    ),
    [isFetchingPreviousPage]
  );
  const footer = useMemo(
    () =>
      hasPreviousPage ? null : <ListFooter text="Start of the trip chat" />,
    [hasPreviousPage]
  );

  if (messages.data === undefined) {
    return <QueryStatus error={messages.error} onRetry={messages.refetch} />;
  }

  return (
    <View style={styles.container}>
      <Animated.View
        style={[styles.lifted, { transform: [{ translateY: liftTranslateY }] }]}
      >
        <KeyboardView style={styles.list}>
          <Chat.List
            data={list.data}
            ref={shadowlistRef}
            style={styles.list}
            renderElement={renderBubble}
            getElementSizeSpec={getSizeSpec}
            onStartReached={list.onStartReached}
            ListHeaderComponent={header}
            ListFooterComponent={footer}
          />
        </KeyboardView>
        <Chat.Input onSend={handleSendMessage} labels={INPUT_LABELS} />
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
  })
);
