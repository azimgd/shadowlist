import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist';
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
        ListFooterComponent={<FooterListItem text="Start of conversation" />}
      />
      <FloatingActionBar
        onPrepend={handlePrepend}
        onAppend={handleAppend}
        onScrollToIndex={handleScrollToIndex}
        dataLength={data.length}
      />
      <MessageInput onSend={handleSendMessage} />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000000',
  },
  list: {
    flex: 1,
    backgroundColor: '#000000',
  },
});
