import { useState, useRef, type CSSProperties } from 'react';
import { type ShadowlistCommands } from 'shadowlist-wasm';
import {
  Chat,
  ListHeader,
  ListFooter,
  colors,
  type ChatMessage,
} from 'shadowlist-utils/web';
import { generateUniqueId, buildChatMessage } from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

export const ChatScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<ChatMessage[]>(() =>
    Array.from({ length: 1000 }, (_, index) => buildChatMessage(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      buildChatMessage(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      buildChatMessage(currentLength + index)
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
    shadowlistRef.current?.scrollToIndex(
      Math.floor(Math.random() * data.length)
    );
  };

  useHeaderActions({
    onPrepend: handlePrepend,
    onAppend: handleAppend,
    onScrollToRandom: handleScrollToRandom,
  });

  return (
    <div style={styles.container}>
      <Chat.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={({ element, index }) => (
          <Chat.Bubble
            index={index}
            text={element.text}
            isFromMe={element.isFromMe}
            imageUrl={element.imageUrl}
            imageUrls={element.imageUrls}
          />
        )}
        ListHeaderComponent={
          <ListHeader title="Chat" subtitle="Inverted list" />
        }
        ListFooterComponent={<ListFooter text="Start of conversation" />}
      />
      <Chat.Input onSend={handleSendMessage} />
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
