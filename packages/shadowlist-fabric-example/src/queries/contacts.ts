import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { shareItemsById } from 'shadowlist-utils';
import type { ContactItem } from 'shadowlist-utils/native';
import {
  createContacts,
  deleteContact,
  fetchContacts,
  fetchFavorites,
  saveFavoritesOrder,
} from '../api/contacts';

/*
 * Contacts and Section List read this one cached query, so a contact deleted on either
 * screen is already gone on the other, without a refetch.
 */
const contactsKey = ['contacts'] as const;

export const useContactsQuery = () =>
  useQuery({
    queryKey: contactsKey,
    queryFn: ({ signal }) => fetchContacts(signal),
    /*
     * Rows keep their identity when contacts are added or removed around them, so only the
     * rows that changed re-render; the default sharing matches rows by position.
     */
    structuralSharing: shareItemsById,
  });

export function useAddContacts() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: createContacts,
    onSuccess: (created, { position }) =>
      queryClient.setQueryData<ContactItem[]>(
        contactsKey,
        (data) =>
          data &&
          (position === 'start' ? [...created, ...data] : [...data, ...created])
      ),
  });
}

export function useDeleteContact() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: deleteContact,
    onMutate: async (id) => {
      await queryClient.cancelQueries({ queryKey: contactsKey });
      const previous = queryClient.getQueryData<ContactItem[]>(contactsKey);
      queryClient.setQueryData<ContactItem[]>(contactsKey, (data) =>
        data?.filter((contact) => contact.id !== id)
      );
      return { previous };
    },
    onError: (_error, _id, context) =>
      queryClient.setQueryData(contactsKey, context?.previous),
  });
}

const favoritesKey = ['favorites'] as const;
const reorderFavoritesKey = ['favorites', 'reorder'] as const;

export const useFavoritesQuery = () =>
  useQuery({
    queryKey: favoritesKey,
    queryFn: ({ signal }) => fetchFavorites(signal),
  });

export function useReorderFavorites() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationKey: reorderFavoritesKey,
    mutationFn: (ordered: ContactItem[]) =>
      saveFavoritesOrder(ordered.map((contact) => contact.id)),
    // The dropped order is the new data right away; the list must not snap back mid-save.
    onMutate: async (ordered) => {
      await queryClient.cancelQueries({ queryKey: favoritesKey });
      const previous = queryClient.getQueryData<ContactItem[]>(favoritesKey);
      queryClient.setQueryData(favoritesKey, ordered);
      return { previous };
    },
    onError: (_error, _ordered, context) =>
      queryClient.setQueryData(favoritesKey, context?.previous),
    /*
     * Resync once the last drag of a burst has settled. Refetching after an earlier one
     * could return an order that a later, still-saving drag has already replaced.
     */
    onSettled: () => {
      if (queryClient.isMutating({ mutationKey: reorderFavoritesKey }) === 1) {
        return queryClient.invalidateQueries({ queryKey: favoritesKey });
      }
      return undefined;
    },
  });
}
