import { useState, useRef, type CSSProperties } from 'react';
import { Shadowlist, type ShadowListCommands } from 'shadowlist-wasm';
import { FloatingActionBar } from './FloatingActionBar';
import { ContactElement, type ContactElement as ContactElementType } from './ContactElement';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { generateContact } from './constants';

export const ContactsScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const [data, setData] = useState<ContactElementType[]>(() =>
    Array.from({ length: 100 }, (_, index) => generateContact(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateContact(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateContact(currentLength + index)
    );
    setData((prev) => [...prev, ...newElements]);
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
        renderElement={({ element, index }) => (
          <ContactElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem title="Contacts" subtitle="Swipe left to delete" />
        }
        ListFooterComponent={<FooterListItem text={`${data.length} contacts`} />}
      />
      <FloatingActionBar
        onPrepend={handlePrepend}
        onAppend={handleAppend}
        onScrollToIndex={handleScrollToIndex}
        dataLength={data.length}
      />
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
