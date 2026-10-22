import { useCallback, useEffect, useRef, useMemo } from 'react';
import { Platform, View } from 'react-native';
import {
  type ShadowListCommands,
  type ViewableItemsChangedInfo,
} from 'shadowlist';
import { useInfiniteListProps } from 'shadowlist-utils';
import {
  Feed,
  ListFooter,
  Spinner,
  useTheme,
  type FeedItem,
} from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { benchOverscan } from '../launchSettings';
import { useFeedQuery, usePublishPosts, useRefreshFeed } from '../queries/feed';
import { recallFeed, rememberFeed } from '../shadowhash/feedCache';
import { feedItemFromHash } from '../shadowhash/feedHash';

const PUBLISH_COUNT = 10;
const CACHE_KEY = 'feed';

export const FeedScreen = () => {
  const styles = useScreenStyles();
  const { colors } = useTheme();
  const shadowlistRef = useRef<ShadowListCommands>(null);

  const feed = useFeedQuery();
  const refreshFeed = useRefreshFeed();
  const list = useInfiniteListProps(feed, { refresh: refreshFeed });
  const { mutate: publishPosts } = usePublishPosts();

  // Rows of the last session's first page, drawn while this session's first page loads.
  const cached = useMemo(() => recallFeed(CACHE_KEY), []);
  const firstPage = feed.data?.pages[0];
  useEffect(() => {
    if (firstPage) rememberFeed(CACHE_KEY, firstPage.items);
  }, [firstPage]);

  // The next page's rows at their real size, sent with the last loaded page.
  const lastPage = feed.data?.pages[feed.data.pages.length - 1];
  const { hasNextPage } = feed;
  const upcoming = useMemo(
    () =>
      hasNextPage && lastPage ? lastPage.upcoming.map(feedItemFromHash) : [],
    [hasNextPage, lastPage]
  );
  const data = useMemo(
    () =>
      feed.data === undefined
        ? cached
        : upcoming.length > 0
          ? [...list.data, ...upcoming]
          : list.data,
    [feed.data, cached, upcoming, list.data]
  );

  // The placeholders sit past the real end. The next page loads once the first one shows.
  const { onEndReached } = list;
  const onViewableItemsChanged = useCallback(
    ({ viewableItems }: ViewableItemsChangedInfo<FeedItem>) => {
      if (viewableItems.some((token) => token.item.placeholder)) onEndReached();
    },
    [onEndReached]
  );

  useHeaderActions(
    {
      onPrepend: () => publishPosts(PUBLISH_COUNT),
      onAppend: list.onEndReached,
      onScrollToRandom: () =>
        shadowlistRef.current?.scrollToIndex({
          index: Math.floor(Math.random() * list.data.length),
          animated: false,
        }),
      prependLabel: 'Publish New Posts',
      appendLabel: 'Load More Posts',
    },
    Platform.OS === 'macos'
      ? [
          [
            {
              label: 'Refresh',
              symbol: 'arrow.clockwise',
              onPress: list.onRefresh,
            },
          ],
        ]
      : []
  );

  const footer = useMemo(
    () =>
      upcoming.length > 0 ? null : hasNextPage ? (
        <Spinner />
      ) : (
        <ListFooter text="You're all caught up" />
      ),
    [hasNextPage, upcoming.length]
  );

  if (feed.data === undefined && (cached.length === 0 || feed.error)) {
    return <QueryStatus error={feed.error} onRetry={feed.refetch} />;
  }

  return (
    <View style={styles.container}>
      <Feed.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        refreshing={list.refreshing}
        onRefresh={list.onRefresh}
        refreshColor={colors.secondaryLabel}
        onEndReached={list.onEndReached}
        onViewableItemsChanged={onViewableItemsChanged}
        ListFooterComponent={footer}
        mountOverscanRows={benchOverscan}
        mountOverscanRowsLeading={benchOverscan}
      />
    </View>
  );
};
