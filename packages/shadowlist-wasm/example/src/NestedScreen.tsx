import { useState, useRef, type CSSProperties } from 'react';
import { Shadowlist, type ShadowListCommands } from 'shadowlist-wasm';
import { FloatingActionBar } from './FloatingActionBar';
import { NestedElement, type NestedElement as NestedElementType } from './NestedElement';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { generateNestedElement } from './constants';

export const NestedScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const [data, setData] = useState<NestedElementType[]>(() =>
    Array.from({ length: 20 }, (_, index) => generateNestedElement(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 5 }, (_, index) =>
      generateNestedElement(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 5 }, (_, index) =>
      generateNestedElement(currentLength + index)
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
          <NestedElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem title="Nested" subtitle="Nested horizontal lists" />
        }
        ListFooterComponent={<FooterListItem text="End of nested list" />}
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
