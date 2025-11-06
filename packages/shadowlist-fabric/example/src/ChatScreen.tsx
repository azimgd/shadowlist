import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowListCommands } from 'shadowlist';
import { ChatElement } from './ChatElement';
import { FloatingActionBar } from './FloatingActionBar';
import { MessageInput } from './MessageInput';
import { generateUniqueId, generateRandomText, generateOptimizedImageUrl, shouldBeImageGrid, generateImageGrid } from './constants';

export const ChatScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const [data, setData] = useState<{id: string, text: string, isFromMe: boolean, imageUrl?: string, imageUrls?: string[]}[]>(() =>
    Array.from({ length: 1000 }, (_, index) => {
      const isImageGrid = shouldBeImageGrid(index);
      const imageUrl = generateOptimizedImageUrl(index);

      return {
        id: generateUniqueId(),
        text: (isImageGrid || !!imageUrl) ? '' : generateRandomText(index),
        isFromMe: index % 3 !== 0,
        imageUrl: imageUrl,
        imageUrls: isImageGrid ? generateImageGrid(index) : undefined,
      };
    })
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) => {
      const elementIndex = currentLength + index;
      const isImageGrid = shouldBeImageGrid(elementIndex);
      const imageUrl = generateOptimizedImageUrl(elementIndex);

      return {
        id: generateUniqueId(),
        text: (isImageGrid || !!imageUrl) ? '' : generateRandomText(elementIndex),
        isFromMe: elementIndex % 3 !== 0,
        imageUrl: imageUrl,
        imageUrls: isImageGrid ? generateImageGrid(elementIndex) : undefined,
      };
    });
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) => {
      const elementIndex = currentLength + index;
      const isImageGrid = shouldBeImageGrid(elementIndex);
      const imageUrl = generateOptimizedImageUrl(elementIndex);

      return {
        id: generateUniqueId(),
        text: (isImageGrid || !!imageUrl) ? '' : generateRandomText(elementIndex),
        isFromMe: elementIndex % 3 !== 0,
        imageUrl: imageUrl,
        imageUrls: isImageGrid ? generateImageGrid(elementIndex) : undefined,
      };
    });
    setData((prev) => [...prev, ...newElements]);
  };

  const handleSendMessage = (message: string) => {
    const newMessage = {
      id: generateUniqueId(),
      text: message,
      isFromMe: true,
      imageUrl: undefined,
    };
    setData((prev) => [...prev, newMessage]);
  };

  return (
    <View style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderItem={({ item: element, index }) => (
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
      />
      <FloatingActionBar onPrepend={handlePrepend} onAppend={handleAppend} />
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
