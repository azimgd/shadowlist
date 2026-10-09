import { fetchFeedPage, publishPosts } from '../api/feed';
import {
  useCursorInfiniteQuery,
  usePrependMutation,
  useRefreshFirstPage,
} from './infinite';

const feedKey = ['feed'] as const;

export const useFeedQuery = () =>
  useCursorInfiniteQuery({ queryKey: feedKey, fetchPage: fetchFeedPage });

export const useRefreshFeed = () => useRefreshFirstPage(feedKey);

export const usePublishPosts = () => usePrependMutation(feedKey, publishPosts);
