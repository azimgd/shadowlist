import { useState, useRef, type CSSProperties } from 'react';
import { type ShadowlistCommands } from 'shadowlist-wasm';
import { Nested, ListHeader, ListFooter, colors } from 'shadowlist-utils/web';
import { generateNestedElement, type NestedItem } from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

export const NestedScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<NestedItem[]>(() =>
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
      <Nested.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={({ element }) => <Nested.Row element={element} />}
        ListHeaderComponent={
          <ListHeader title="Nested" subtitle="Nested horizontal lists" />
        }
        ListFooterComponent={<ListFooter text="End of nested list" />}
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
