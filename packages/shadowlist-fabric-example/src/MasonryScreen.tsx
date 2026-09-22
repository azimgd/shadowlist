import { useMemo, useRef } from 'react';
import { View } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import { useInfiniteListProps } from 'shadowlist-utils';
import { Masonry, ListFooter, Spinner } from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { usePhotosQuery, usePublishPhotos } from './queries/gallery';

export const MasonryScreen = () => {
  const styles = useScreenStyles();
  const shadowlistRef = useRef<ShadowListCommands>(null);

  const photos = usePhotosQuery();
  const list = useInfiniteListProps(photos);
  const { mutate: publishPhotos } = usePublishPhotos();

  useHeaderActions({
    onPrepend: () => publishPhotos(10),
    onAppend: list.onEndReached,
    onScrollToRandom: () =>
      shadowlistRef.current?.scrollToIndex(
        Math.floor(Math.random() * list.data.length)
      ),
    prependLabel: 'Publish New Photos',
    appendLabel: 'Load More Photos',
  });

  const { hasNextPage } = photos;
  const footer = useMemo(
    () =>
      hasNextPage ? <Spinner /> : <ListFooter text="End of the gallery" />,
    [hasNextPage]
  );

  if (photos.data === undefined) {
    return <QueryStatus error={photos.error} onRetry={photos.refetch} />;
  }

  return (
    <View style={styles.container}>
      <Masonry.List
        data={list.data}
        ref={shadowlistRef}
        style={styles.list}
        onEndReached={list.onEndReached}
        ListFooterComponent={footer}
      />
    </View>
  );
};
