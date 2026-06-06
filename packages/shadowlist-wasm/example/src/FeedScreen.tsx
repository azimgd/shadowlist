import { useState, useRef, useCallback, useMemo, type CSSProperties } from 'react';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist-wasm';
import { useHeaderActions } from './HeaderActions';
import { FeedElement, type FeedElement as FeedElementType } from './FeedElement';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { Spinner } from './Spinner';
import { generateFeedElement } from './constants';
import { colors } from './theme';

export const FeedScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<FeedElementType[]>(() =>
    Array.from({ length: 1000 }, (_, index) => generateFeedElement(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateFeedElement(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateFeedElement(currentLength + index)
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

  // Pull-to-refresh: prepend a fresh batch after a short delay (stand-in fetch).
  const [refreshing, setRefreshing] = useState(false);
  const handleRefresh = useCallback(() => {
    setRefreshing(true);
    window.setTimeout(() => {
      setData((prev) => [
        ...Array.from({ length: 10 }, (_, index) => generateFeedElement(index)),
        ...prev,
      ]);
      setRefreshing(false);
    }, 1200);
  }, []);

  // Infinite scroll: append the next page when the end is reached.
  const [loadingMore, setLoadingMore] = useState(false);
  const loadingRef = useRef(false);
  const handleEndReached = useCallback(() => {
    if (loadingRef.current) return;
    loadingRef.current = true;
    setLoadingMore(true);
    window.setTimeout(() => {
      setData((prev) => [
        ...prev,
        ...Array.from({ length: 20 }, (_, index) =>
          generateFeedElement(prev.length + index)
        ),
      ]);
      setLoadingMore(false);
      loadingRef.current = false;
    }, 1000);
  }, []);

  const footer = useMemo(
    () =>
      loadingMore ? <Spinner /> : <FooterListItem text="End of feed" />,
    [loadingMore]
  );

  return (
    <div style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        refreshing={refreshing}
        onRefresh={handleRefresh}
        onEndReached={handleEndReached}
        renderElement={({ element, index }) => (
          <FeedElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem title="Feed" subtitle="Vertical scrolling list" />
        }
        ListFooterComponent={footer}
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
