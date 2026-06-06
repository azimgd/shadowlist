import { useState, useRef, type CSSProperties } from 'react';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist-wasm';
import { useHeaderActions } from './HeaderActions';
import { MasonryElement, type MasonryElement as MasonryElementType } from './MasonryElement';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { generateMasonryElement } from './constants';
import { colors } from './theme';

export const MasonryScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<MasonryElementType[]>(() =>
    Array.from({ length: 100 }, (_, index) => generateMasonryElement(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateMasonryElement(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateMasonryElement(currentLength + index)
    );
    setData((prev) => [...prev, ...newElements]);
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
        columns={3}
        renderElement={({ element, index }) => (
          <MasonryElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem title="Masonry" subtitle="Three column grid layout" />
        }
        ListFooterComponent={<FooterListItem text="End of masonry grid" />}
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
    background: colors.background,
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
};
