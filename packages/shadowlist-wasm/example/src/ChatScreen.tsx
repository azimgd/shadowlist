import { useState, useRef, type CSSProperties } from 'react';
import { Shadowlist, type ShadowListCommands } from 'shadowlist-wasm';
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
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const [data, setData] = useState<ChatMessage[]>(() =>
    Array.from({ length: 1000 }, (_, index) => {
      const isImageGrid = shouldBeImageGrid(index);
      const imageUrl = generateOptimizedImageUrl(index);

      return {
        id: generateUniqueId(),
        text: isImageGrid || !!imageUrl ? '' : generateRandomText(index),
        isFromMe: index % 3 !== 0,
        imageUrl,
        imageUrls: isImageGrid ? generateImageGrid(index) : undefined,
      };
    })
  );

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
    <div style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        inverted
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
        ListHeaderComponent={<HeaderListItem title="Chat" subtitle="Inverted list" />}
        ListFooterComponent={<FooterListItem text="Start of conversation" />}
      />
      <FloatingActionBar
        onPrepend={handlePrepend}
        onAppend={handleAppend}
        onScrollToIndex={handleScrollToIndex}
        dataLength={data.length}
      />
      <MessageInput onSend={handleSendMessage} />
    </div>
  );
};

const styles: Record<string, CSSProperties> = {
  container: {
    position: 'relative',
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minHeight: 0,
    background: '#000000',
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: '#000000',
  },
};
