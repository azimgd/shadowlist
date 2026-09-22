import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from 'react';
import { Alert, Animated, StyleSheet, Text, View } from 'react-native';
import {
  KeyboardView,
  ShadowListNative,
  type ShadowListNativeCommands,
  type ShadowListNativeElementPressEvent,
} from 'shadowlist';
import {
  Chat,
  ListFooter,
  ListHeader,
  Spinner,
  createStyles,
  defaultChatLabels,
  useKeyboardLift,
  useTheme,
  type ChatMessage,
  type ChatMessageStatus,
} from 'shadowlist-utils/native';
import { formatOrdinalLabel } from './itemOrdinals';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import {
  createOutgoingMessage,
  fetchChatPage,
  sendChatMessage,
  simulateIncomingMessages,
  subscribeToIncomingMessages,
} from './api/chat';

const KEYBOARD_GAP = 8;
const INCOMING_COUNT = 10;
const INPUT_LABELS = { placeholder: 'Message your crew' };
const AVATAR_SIZE = 30;
const GRID_LIMIT = 4;

type ChatTemplate =
  | 'sent'
  | 'received'
  | 'sentImage'
  | 'receivedImage'
  | 'sentGrid'
  | 'receivedGrid';

/*
 * A ChatMessage flattened into what the bubble templates bind. There is one template per side
 * and content kind, and every visible string is computed up front.
 */
interface ChatNativeRow {
  id: string;
  type: ChatTemplate;
  text: string;
  name: string;
  initials: string;
  avatarColor: string;
  image: string;
  images: string[];
  caption: string;
  // The Sending or Sent suffix after the caption of a message sent from this screen.
  statusLabel: string;
  opacity: number;
  failed: boolean;
}

function getInitials(name: string): string {
  const words = name.trim().split(/\s+/).filter(Boolean);
  const first = words[0] ?? '';
  const last = words.length > 1 ? words[words.length - 1]! : '';
  return `${[...first][0] ?? ''}${[...last][0] ?? ''}`.toUpperCase();
}

function getAvatarColor(name: string, palette: ReadonlyArray<string>): string {
  let hash = 0;
  for (let index = 0; index < name.length; index++) {
    hash = (hash * 31 + name.charCodeAt(index)) % 2147483647;
  }
  return palette[hash % palette.length] ?? '#888888';
}

const STATUS_LABELS: Partial<Record<ChatMessageStatus, string>> = {
  sending: ' · Sending',
  sent: ' · Sent',
};

/*
 * The fields a status change rewrites. The row is patched with just these.
 */
function statusFields(status: ChatMessageStatus | undefined, tracked: boolean) {
  return {
    statusLabel: tracked && status ? (STATUS_LABELS[status] ?? '') : '',
    opacity: status === 'sending' ? 0.6 : 1,
    failed: status === 'failed',
  };
}

function toRow(
  message: ChatMessage,
  caption: string,
  palette: ReadonlyArray<string>,
  tracked = false
): ChatNativeRow {
  const images = (message.images ?? []).slice(0, GRID_LIMIT);
  const kind =
    images.length === 0 ? '' : images.length === 1 ? 'Image' : 'Grid';
  return {
    id: message.id,
    type: `${message.isOwn ? 'sent' : 'received'}${kind}` as ChatTemplate,
    text: message.text ?? '',
    name: message.author.name,
    initials: getInitials(message.author.name),
    avatarColor:
      message.author.avatarColor ??
      getAvatarColor(message.author.name, palette),
    image: images[0] ?? '',
    images,
    caption,
    ...statusFields(message.status, tracked),
  };
}

/*
 * Position labels under each bubble, as on the Chat screen. The first page counts forward,
 * appended messages continue it, and each page of history counts away from it.
 */
function useOrdinals() {
  const counts = useRef({ forward: 0, prepended: 0 });
  return useMemo(
    () => ({
      forward: () => formatOrdinalLabel(++counts.current.forward),
      prepended: () => formatOrdinalLabel(++counts.current.prepended, true),
    }),
    []
  );
}

/*
 * Chat on ShadowListNative, an inverted list whose bubbles are cloned natively from six templates.
 * The thread lives in the list's native store. Every change after the first page is a row
 * command, with no re-render.
 */
export const ChatNativeScreen = () => {
  const listRef = useRef<ShadowListNativeCommands<ChatNativeRow>>(null);
  const theme = useTheme();
  const { colors } = theme;
  const styles = useStyles();
  const liftTranslateY = useKeyboardLift({ gap: KEYBOARD_GAP });
  const ordinals = useOrdinals();

  const [initialRows, setInitialRows] = useState<ChatNativeRow[] | null>(null);
  const [error, setError] = useState<Error | null>(null);
  const [loadingEarlier, setLoadingEarlier] = useState(false);
  const [hasEarlier, setHasEarlier] = useState(true);
  const cursorRef = useRef<number | undefined>(undefined);
  const loadingRef = useRef(false);
  // Messages sent from this screen, kept for retries.
  const outgoingRef = useRef(new Map<string, ChatMessage>());

  const palette = colors.avatarPalette;

  const loadFirstPage = useCallback(() => {
    setError(null);
    fetchChatPage(undefined).then(
      (page) => {
        cursorRef.current = page.previousCursor;
        setHasEarlier(page.previousCursor !== undefined);
        setInitialRows(
          page.items.map((message) =>
            toRow(message, ordinals.forward(), palette)
          )
        );
      },
      (failure: Error) => setError(failure)
    );
  }, [ordinals, palette]);

  useEffect(() => {
    loadFirstPage();
    // Load the first page once. Color changes do not refetch.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const loadEarlier = useCallback(() => {
    const cursor = cursorRef.current;
    if (loadingRef.current || cursor === undefined || !initialRows) return;
    loadingRef.current = true;
    setLoadingEarlier(true);
    fetchChatPage({ before: cursor })
      .then((page) => {
        cursorRef.current = page.previousCursor;
        setHasEarlier(page.previousCursor !== undefined);
        // Numbered from the newest in the page outwards, away from the loaded thread.
        const captions = page.items.map(() => '');
        for (let index = page.items.length - 1; index >= 0; index--) {
          captions[index] = ordinals.prepended();
        }
        listRef.current?.prependItems(
          page.items.map((message, index) =>
            toRow(message, captions[index]!, palette)
          )
        );
      })
      .catch(() => {})
      .finally(() => {
        loadingRef.current = false;
        setLoadingEarlier(false);
      });
  }, [initialRows, ordinals, palette]);

  // Incoming messages keep the reader in place, as on Chat.
  useEffect(
    () =>
      subscribeToIncomingMessages((messages) => {
        listRef.current?.appendItems(
          messages.map((message) => toRow(message, ordinals.forward(), palette))
        );
      }),
    [ordinals, palette]
  );

  const deliver = useCallback((message: ChatMessage) => {
    const list = listRef.current;
    list?.updateItem(message.id, statusFields('sending', true));
    sendChatMessage(message).then(
      () => listRef.current?.updateItem(message.id, statusFields('sent', true)),
      () => {
        listRef.current?.updateItem(message.id, statusFields('failed', true));
        // The retry line grows the newest bubble, so keep it above the composer.
        const keys = listRef.current?.getKeys() ?? [];
        if (keys[keys.length - 1] === message.id) {
          listRef.current?.scrollToEnd();
        }
      }
    );
  }, []);

  const handleSend = useCallback(
    (text: string) => {
      const message = createOutgoingMessage(text);
      outgoingRef.current.set(message.id, message);
      listRef.current?.appendItems([
        toRow(message, ordinals.forward(), palette, true),
      ]);
      // Bring the reader's own message into view. Its row is already in the store.
      listRef.current?.scrollToEnd();
      deliver(message);
    },
    [deliver, ordinals, palette]
  );

  const handleElementPress = useCallback(
    ({
      key,
      action,
      item,
    }: ShadowListNativeElementPressEvent<ChatNativeRow>) => {
      if (action === 'retry') {
        const message = outgoingRef.current.get(key);
        if (message) deliver(message);
        return;
      }
      if (action !== 'message') return;
      const preview = item?.text ? `“${item.text.slice(0, 80)}”` : 'Image';
      Alert.alert('Message', preview, [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: () => listRef.current?.removeItems([key]),
        },
      ]);
    },
    [deliver]
  );

  useHeaderActions({
    onPrepend: loadEarlier,
    onAppend: () => simulateIncomingMessages(INCOMING_COUNT),
    onScrollToRandom: () =>
      listRef.current?.scrollToIndex(
        Math.floor(Math.random() * (listRef.current?.getCount() ?? 1))
      ),
  });

  const templates = useMemo(() => {
    const avatar = (
      <ShadowListNative.View
        style={styles.avatar}
        bind={{ backgroundColor: 'avatarColor' }}
      >
        <ShadowListNative.Text
          style={styles.initials}
          bind={{ text: 'initials' }}
        />
      </ShadowListNative.View>
    );
    const failedLine = (
      <ShadowListNative.View
        action="retry"
        style={styles.alignEnd}
        bind={{ visible: 'failed' }}
      >
        <Text style={styles.failed}>{defaultChatLabels.failed}</Text>
      </ShadowListNative.View>
    );
    const captionLine = (own: boolean) => (
      <ShadowListNative.Text
        style={[styles.captionLine, own ? styles.alignEnd : styles.alignStart]}
        bind={{ text: '{caption}{statusLabel}' }}
      />
    );
    const textBubble = (own: boolean) => (
      <View style={styles.bubbleColumn}>
        {own ? null : (
          <ShadowListNative.Text
            style={styles.sender}
            bind={{ text: 'name' }}
          />
        )}
        <ShadowListNative.View
          id="bubble"
          action="message"
          style={[styles.bubble, own ? styles.bubbleOwn : styles.bubbleOther]}
          bind={own ? { opacity: 'opacity' } : undefined}
        >
          <ShadowListNative.Text
            style={[styles.text, own && styles.textOwn]}
            bind={{ text: 'text' }}
          />
          <ShadowListNative.Text
            style={[styles.captionInline, own && styles.captionInlineOwn]}
            bind={{ text: '{caption}{statusLabel}' }}
          />
        </ShadowListNative.View>
        {own ? failedLine : null}
      </View>
    );
    const imageBubble = (own: boolean) => (
      <View>
        <ShadowListNative.View
          action="message"
          style={styles.singleImageContainer}
        >
          <ShadowListNative.Image
            style={styles.image}
            resizeMode="cover"
            bind={{ uri: 'image' }}
          />
        </ShadowListNative.View>
        {captionLine(own)}
        {own ? failedLine : null}
      </View>
    );
    const gridBubble = (own: boolean) => (
      <View style={styles.imageGridColumn}>
        <ShadowListNative.View
          action="message"
          style={styles.imageGrid}
          repeat="images"
          repeatMax={GRID_LIMIT}
        >
          <View style={styles.imageGridCell}>
            <ShadowListNative.Image
              style={styles.image}
              resizeMode="cover"
              bind={{ uri: '.' }}
            />
          </View>
        </ShadowListNative.View>
        {captionLine(own)}
        {own ? failedLine : null}
      </View>
    );
    const row = (own: boolean, content: ReactNode) => (
      <View
        style={[
          styles.container,
          own ? styles.containerOwn : styles.containerOther,
        ]}
      >
        {own ? null : avatar}
        {content}
      </View>
    );
    return {
      sent: row(true, textBubble(true)),
      received: row(false, textBubble(false)),
      sentImage: row(true, imageBubble(true)),
      receivedImage: row(false, imageBubble(false)),
      sentGrid: row(true, gridBubble(true)),
      receivedGrid: row(false, gridBubble(false)),
    };
  }, [styles]);

  const header = useMemo(
    () => (
      <View>
        <ListHeader
          title="Lisbon Crew (Native)"
          subtitle="8 travellers · departs Oct 12"
        />
        {loadingEarlier ? <Spinner size={16} /> : null}
      </View>
    ),
    [loadingEarlier]
  );
  const footer = useMemo(
    () => (hasEarlier ? null : <ListFooter text="Start of the trip chat" />),
    [hasEarlier]
  );

  if (initialRows === null) {
    return <QueryStatus error={error} onRetry={loadFirstPage} />;
  }

  return (
    <View style={styles.screen}>
      <Animated.View
        style={[styles.lifted, { transform: [{ translateY: liftTranslateY }] }]}
      >
        <KeyboardView style={styles.list}>
          <ShadowListNative
            ref={listRef}
            initialData={initialRows}
            templates={templates}
            templateKey="type"
            inverted
            style={styles.list}
            onStartReached={loadEarlier}
            onElementPress={handleElementPress}
            ListHeaderComponent={header}
            ListFooterComponent={footer}
          />
        </KeyboardView>
        <Chat.Input onSend={handleSend} labels={INPUT_LABELS} />
      </Animated.View>
    </View>
  );
};

const useStyles = createStyles(
  ({ colors, typography, spacing, radius, fontWeight }) =>
    StyleSheet.create({
      screen: {
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
      container: {
        paddingHorizontal: spacing.md,
        paddingVertical: spacing.xxs,
        flexDirection: 'row',
        alignItems: 'flex-end',
      },
      containerOwn: {
        justifyContent: 'flex-end',
      },
      containerOther: {
        justifyContent: 'flex-start',
      },
      avatar: {
        width: AVATAR_SIZE,
        height: AVATAR_SIZE,
        borderRadius: AVATAR_SIZE / 2,
        alignItems: 'center',
        justifyContent: 'center',
        overflow: 'hidden',
        marginRight: spacing.sm,
        marginBottom: spacing.xxs,
      },
      initials: {
        color: colors.label,
        fontWeight: fontWeight.semibold,
        fontSize: Math.floor(AVATAR_SIZE * 0.43),
      },
      bubbleColumn: {
        maxWidth: '75%',
      },
      sender: {
        color: colors.secondaryLabel,
        ...typography.caption,
        marginLeft: spacing.md,
        marginBottom: spacing.xxs,
      },
      bubble: {
        paddingHorizontal: 14,
        paddingVertical: spacing.sm,
        borderRadius: radius.lg + 2,
      },
      bubbleOwn: {
        backgroundColor: colors.blue,
        borderBottomRightRadius: 5,
        alignSelf: 'flex-end',
      },
      bubbleOther: {
        backgroundColor: colors.elevated2,
        borderBottomLeftRadius: 5,
        alignSelf: 'flex-start',
      },
      text: {
        color: colors.label,
        ...typography.body,
      },
      textOwn: {
        color: colors.onAccent,
      },
      captionInline: {
        color: colors.secondaryLabel,
        ...typography.caption,
        marginTop: spacing.xs,
      },
      captionInlineOwn: {
        color: colors.onAccent,
        opacity: 0.65,
      },
      failed: {
        color: colors.red,
        ...typography.caption,
        marginTop: spacing.xs,
      },
      captionLine: {
        color: colors.secondaryLabel,
        ...typography.caption,
        marginTop: spacing.xs,
      },
      alignEnd: {
        alignSelf: 'flex-end',
      },
      alignStart: {
        alignSelf: 'flex-start',
      },
      singleImageContainer: {
        width: 240,
        height: 320,
        borderRadius: radius.lg,
        overflow: 'hidden',
        backgroundColor: colors.elevated2,
        marginVertical: spacing.xxs,
      },
      imageGrid: {
        width: 240,
        flexDirection: 'row',
        flexWrap: 'wrap',
        gap: spacing.xxs,
        marginVertical: spacing.xxs,
      },
      imageGridColumn: {
        paddingBottom: spacing.xxs,
      },
      imageGridCell: {
        width: 119,
        height: 119,
        borderRadius: radius.sm,
        overflow: 'hidden',
        backgroundColor: colors.elevated2,
      },
      image: {
        width: '100%',
        height: '115%',
      },
    })
);
