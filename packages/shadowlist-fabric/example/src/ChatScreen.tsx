import { useState, useRef, useMemo } from 'react';
import { View, StyleSheet, Animated } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import {
  Shadowlist,
  KeyboardDismissView,
  useKeyboardAnimation,
  type ShadowlistCommands,
} from 'shadowlist';
import { ChatElement } from './ChatElement';
import { FloatingActionBar } from './FloatingActionBar';
import { MessageInput } from './MessageInput';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import {
  generateUniqueId,
  generateRandomText,
  generateOptimizedImageUrl,
  shouldBeImageGrid,
  generateImageGrid,
} from './constants';

type ChatMessage = {
  id: string;
  text: string;
  isFromMe: boolean;
  imageUrl?: string;
  imageUrls?: string[];
};

export const ChatScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const insets = useSafeAreaInsets();

  /*
   * Keyboard avoidance driven by our own native keyboard module (useKeyboardAnimation
   * -> ShadowlistKeyboard TurboModule), which reads the real keyboard frame natively
   * each frame. `height` is the live keyboard height (dp); the list and composer are
   * one unit we translate up by it so the composer sits right above the keyboard and
   * the inverted (bottom-anchored) list keeps its newest message above the composer
   * while older rows slide off the top.
   */
  const { height } = useKeyboardAnimation();

  /*
   * translateY = -max(0, keyboardHeight - safeAreaBottom): the composer already pads
   * the safe area, so we subtract it and clamp at 0 (no lift until the keyboard rises
   * past the home-indicator inset), landing the composer flush against the keyboard.
   * Interpolation does the subtract+clamp in one native-bindable node.
   */
  const liftTranslateY = useMemo(() => {
    const safe = insets.bottom;
    return height.interpolate({
      inputRange: safe > 0 ? [0, safe, safe + 1] : [0, 1],
      outputRange: safe > 0 ? [0, 0, -1] : [0, -1],
    });
  }, [height, insets.bottom]);

  const buildMessage = (elementIndex: number): ChatMessage => {
    const isImageGrid = shouldBeImageGrid(elementIndex);
    const imageUrl = generateOptimizedImageUrl(elementIndex);
    return {
      id: generateUniqueId(),
      text: isImageGrid || !!imageUrl ? '' : generateRandomText(elementIndex),
      isFromMe: elementIndex % 3 !== 0,
      imageUrl,
      imageUrls: isImageGrid ? generateImageGrid(elementIndex) : undefined,
    };
  };

  const [data, setData] = useState<ChatMessage[]>(() =>
    Array.from({ length: 1000 }, (_, index) => buildMessage(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      buildMessage(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      buildMessage(currentLength + index)
    );
    setData((prev) => [...prev, ...newElements]);
  };

  const handleSendMessage = (message: string) => {
    setData((prev) => [
      ...prev,
      { id: generateUniqueId(), text: message, isFromMe: true },
    ]);
  };

  const handleScrollToIndex = (index: number) => {
    shadowlistRef.current?.scrollToIndex(index);
  };

  return (
    <View style={styles.container}>
      <Animated.View
        style={[styles.lifted, { transform: [{ translateY: liftTranslateY }] }]}
      >
        {/*
         * Tapping the messages dismisses the keyboard. The wrapper only intercepts
         * while the keyboard is open, so list scrolling is untouched otherwise. The
         * composer is kept outside it so tapping the input keeps the keyboard up.
         */}
        <KeyboardDismissView style={styles.list}>
          <Shadowlist
            data={data}
            ref={shadowlistRef}
            style={styles.list}
            renderElement={({ element, index }) => (
              <ChatElement
                id={element.id}
                index={index}
                text={element.text}
                isFromMe={element.isFromMe}
                imageUrl={element.imageUrl}
                imageUrls={element.imageUrls}
              />
            )}
            inverted
            ListHeaderComponent={
              <HeaderListItem title="Chat" subtitle="Inverted list" />
            }
            ListFooterComponent={
              <FooterListItem text="Start of conversation" />
            }
          />
        </KeyboardDismissView>
        <MessageInput onSend={handleSendMessage} />
      </Animated.View>
      <FloatingActionBar
        onPrepend={handlePrepend}
        onAppend={handleAppend}
        onScrollToIndex={handleScrollToIndex}
        dataLength={data.length}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000000',
    // Clip the lifted content at the screen-body top so rows sliding up never
    // escape over the navigation header.
    overflow: 'hidden',
  },
  lifted: {
    flex: 1,
    backgroundColor: '#000000',
  },
  list: {
    flex: 1,
    backgroundColor: '#000000',
  },
});
