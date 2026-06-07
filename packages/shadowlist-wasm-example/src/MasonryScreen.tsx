import { useState, useRef, type CSSProperties } from 'react';
import { type ShadowlistCommands } from 'shadowlist-wasm';
import { Masonry, ListHeader, ListFooter, colors } from 'shadowlist-utils/web';
import { generateMasonryElement, type MasonryItem } from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

export const MasonryScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<MasonryItem[]>(() =>
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
      <Masonry.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        columns={3}
        renderElement={({ element }) => <Masonry.Card element={element} />}
        ListHeaderComponent={
          <ListHeader title="Masonry" subtitle="Three column grid layout" />
        }
        ListFooterComponent={<ListFooter text="End of masonry grid" />}
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
