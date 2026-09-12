import { useCallback, useRef, useMemo } from 'react';
import { View, StyleSheet, Animated } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import {
  KeyboardView,
  useKeyboardAnimation,
  type ShadowListCommands,
} from 'shadowlist';
import {
  Chat,
  ListHeader,
  ListFooter,
  colors,
  type ChatMessage,
} from 'shadowlist-utils/native';
import {
  generateUniqueId,
  generateRandomText,
  generateOptimizedImageUrl,
  shouldBeImageGrid,
  generateImageGrid,
  generateAvatar,
  useListController,
} from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

// Gap kept between the composer and the keyboard; matches the composer's top padding.
const KEYBOARD_GAP = 8;

const buildMessage = (elementIndex: number): ChatMessage => {
  const isImageGrid = shouldBeImageGrid(elementIndex);
  const imageUrl = generateOptimizedImageUrl(elementIndex);
  const avatar = generateAvatar(elementIndex);
  return {
    id: generateUniqueId(),
    text: isImageGrid || !!imageUrl ? '' : generateRandomText(elementIndex),
    isFromMe: elementIndex % 3 !== 0,
    imageUrl,
    imageUrls: isImageGrid ? generateImageGrid(elementIndex) : undefined,
    username: avatar.name,
    avatarColor: avatar.color,
    initials: avatar.initials,
  };
};

/*
 * What a text bubble measures to, derived from ChatBubble's own styles.
 *
 * Native computes the real height from this before React renders the row, so the list has
 * correct geometry on the first frame instead of estimating every bubble and reflowing the
 * ones below it as each is laid out -- which on an inverted chat list also means the scroll
 * position stops being nudged while you read.
 *
 * The numbers below are a transcription of ChatBubble's stylesheet, and they have to stay
 * one: a spec that disagrees with the row it describes produces a confidently wrong height,
 * which is worse than no prediction at all. Anything the styles cannot express exactly --
 * here, the image rows -- returns null and is estimated and measured the ordinary way.
 *
 *   container   paddingHorizontal 12, paddingVertical 2
 *   avatar      30 wide + 8 margin, only on messages from others
 *   bubbleColumn maxWidth 75% of the container's content box
 *   sender      caption line (16) + 2 margin, only on messages from others
 *   bubble      paddingHorizontal 14, paddingVertical 8
 *   text        typography.body: 17/22, letterSpacing -0.43
 */
const BUBBLE_WIDTH_FRACTION = 0.75;
// 0.75 x (W - 24) - 28  ==  0.75 x W - 46
const BUBBLE_INSET_WIDTH = 46;
// container 2+2, bubble 8+8
const BUBBLE_INSET_HEIGHT = 20;
// sender caption line + its bottom margin, on messages from others only
const SENDER_HEIGHT = 18;

const measureElement = (element: ChatMessage) => {
  /*
   * An image row's height comes from the images, not from text. There is nothing honest to
   * predict, so say so and let the core measure it.
   */
  if (!element.text) return null;

  return {
    text: element.text,
    fontSize: 17,
    lineHeight: 22,
    letterSpacing: -0.43,
    widthFraction: BUBBLE_WIDTH_FRACTION,
    insetWidth: BUBBLE_INSET_WIDTH,
    insetHeight: BUBBLE_INSET_HEIGHT + (element.isFromMe ? 0 : SENDER_HEIGHT),
  };
};

export const ChatScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const insets = useSafeAreaInsets();

  // Live keyboard height (dp); the list and composer translate up by it.
  const { height } = useKeyboardAnimation();

  // Lift the composer to rest KEYBOARD_GAP above the keyboard once it passes the safe-area
  // inset, so the input keeps the same gap below it as its top padding (not flush).
  const liftTranslateY = useMemo(() => {
    const safe = insets.bottom;
    return height.interpolate({
      inputRange: safe > 0 ? [0, safe, safe + 1] : [0, 1, 2],
      outputRange: [0, -KEYBOARD_GAP, -KEYBOARD_GAP - 1],
    });
  }, [height, insets.bottom]);

  const initialData = useMemo(
    () => Array.from({ length: 20000 }, (_, index) => buildMessage(index)),
    []
  );
  const list = useListController<ChatMessage>({ initialData });

  const handlePrepend = () =>
    list.prepend(
      Array.from({ length: 10 }, (_, index) =>
        buildMessage(list.data.length + index)
      )
    );
  const handleAppend = () =>
    list.append(
      Array.from({ length: 10 }, (_, index) =>
        buildMessage(list.data.length + index)
      )
    );
  const handleSendMessage = (message: string) =>
    list.append([{ id: generateUniqueId(), text: message, isFromMe: true }]);
  const handleScrollToRandom = () =>
    shadowlistRef.current?.scrollToIndex(
      Math.floor(Math.random() * list.data.length)
    );

  useHeaderActions({
    onPrepend: handlePrepend,
    onAppend: handleAppend,
    onScrollToRandom: handleScrollToRandom,
  });

  const renderElement = useCallback(
    ({ element }: { element: ChatMessage }) => (
      <Chat.Bubble
        text={element.text}
        isFromMe={element.isFromMe}
        imageUrl={element.imageUrl}
        imageUrls={element.imageUrls}
        username={element.username}
        avatarColor={element.avatarColor}
        initials={element.initials}
      />
    ),
    []
  );

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
            renderElement={renderElement}
            measureElement={measureElement}
            ListHeaderComponent={
              <ListHeader title="Chat" subtitle="Inverted list" />
            }
            ListFooterComponent={<ListFooter text="Start of conversation" />}
            overscan={3}
            materializationOverscan={0.5}
          />
        </KeyboardView>
        <Chat.Input onSend={handleSendMessage} />
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
});
