import { useRef, useMemo } from 'react';
import { View } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import { useInfiniteListProps } from 'shadowlist-utils';
import { Feed, ListFooter, Spinner, useTheme } from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { benchOverscan } from './launchSettings';
import { useFeedQuery, usePublishPosts, useRefreshFeed } from './queries/feed';

const PUBLISH_COUNT = 10;

export const FeedScreen = () => {
  const styles = useScreenStyles();
  const { colors } = useTheme();
  const shadowlistRef = useRef<ShadowListCommands>(null);

  const feed = useFeedQuery();
  const refreshFeed = useRefreshFeed();
  const list = useInfiniteListProps(feed, { refresh: refreshFeed });
  const { mutate: publishPosts } = usePublishPosts();

  useHeaderActions({
    onPrepend: () => publishPosts(PUBLISH_COUNT),
    onAppend: list.onEndReached,
    onScrollToRandom: () =>
      shadowlistRef.current?.scrollToIndex(
        Math.floor(Math.random() * list.data.length)
      ),
    prependLabel: 'Publish New Posts',
    appendLabel: 'Load More Posts',
  });

  const { hasNextPage } = feed;
  const footer = useMemo(
    () =>
      hasNextPage ? <Spinner /> : <ListFooter text="You're all caught up" />,
    [hasNextPage]
  );

  if (feed.data === undefined) {
    return <QueryStatus error={feed.error} onRetry={feed.refetch} />;
  }

  return (
    <View style={styles.container}>
      <Feed.List
        data={list.data}
        ref={shadowlistRef}
        style={styles.list}
        refreshing={list.refreshing}
        onRefresh={list.onRefresh}
        refreshColor={colors.secondaryLabel}
        onEndReached={list.onEndReached}
        ListFooterComponent={footer}
        overscanRows={benchOverscan}
        overscanRowsLeading={benchOverscan}
      />
    </View>
  );
};
