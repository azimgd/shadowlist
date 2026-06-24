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
    () => Array.from({ length: 1000 }, (_, index) => buildMessage(index)),
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
            ListHeaderComponent={
              <ListHeader title="Chat" subtitle="Inverted list" />
            }
            ListFooterComponent={<ListFooter text="Start of conversation" />}
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
