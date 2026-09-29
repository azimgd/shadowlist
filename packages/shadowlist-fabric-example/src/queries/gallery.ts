import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import type { ReorderTileItem } from 'shadowlist-utils/native';
import {
  fetchPhotosPage,
  fetchShelvesPage,
  fetchWishlist,
  publishPhotos,
  saveWishlistOrder,
} from '../api/gallery';
import { useCursorInfiniteQuery, usePrependMutation } from './infinite';

const photosKey = ['gallery', 'photos'] as const;
const shelvesKey = ['gallery', 'shelves'] as const;

export const usePhotosQuery = () =>
  useCursorInfiniteQuery({ queryKey: photosKey, fetchPage: fetchPhotosPage });

export const usePublishPhotos = () =>
  usePrependMutation(photosKey, publishPhotos);

export const useShelvesQuery = () =>
  useCursorInfiniteQuery({ queryKey: shelvesKey, fetchPage: fetchShelvesPage });

const wishlistKey = ['gallery', 'wishlist'] as const;
const reorderWishlistKey = ['gallery', 'wishlist', 'reorder'] as const;

export const useWishlistQuery = () =>
  useQuery({
    queryKey: wishlistKey,
    queryFn: ({ signal }) => fetchWishlist(signal),
  });

// Same flow as useReorderFavorites: the dropped order shows at once and resyncs after a burst.
export function useReorderWishlist() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationKey: reorderWishlistKey,
    mutationFn: (ordered: ReorderTileItem[]) =>
      saveWishlistOrder(ordered.map((item) => item.id)),
    onMutate: async (ordered) => {
      await queryClient.cancelQueries({ queryKey: wishlistKey });
      const previous = queryClient.getQueryData<ReorderTileItem[]>(wishlistKey);
      queryClient.setQueryData(wishlistKey, ordered);
      return { previous };
    },
    onError: (_error, _ordered, context) =>
      queryClient.setQueryData(wishlistKey, context?.previous),
    onSettled: () => {
      if (queryClient.isMutating({ mutationKey: reorderWishlistKey }) === 1) {
        return queryClient.invalidateQueries({ queryKey: wishlistKey });
      }
      return undefined;
    },
  });
}
