import { useState, useRef, type CSSProperties } from 'react';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist-wasm';
import { ChatElement } from './ChatElement';
import { useHeaderActions } from './HeaderActions';
import { MessageInput } from './MessageInput';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { colors } from './theme';
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

export const ChatScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
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

  const handleScrollToRandom = () => {
    shadowlistRef.current?.scrollToIndex(Math.floor(Math.random() * data.length));
  };

  useHeaderActions({
    onPrepend: handlePrepend,
    onAppend: handleAppend,
    onScrollToRandom: handleScrollToRandom,
  });

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
    background: colors.background,
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
};
