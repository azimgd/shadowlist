import {
  fetchPhotosPage,
  fetchShelvesPage,
  publishPhotos,
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
