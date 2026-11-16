import { LayoutAnimation } from 'react-native';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import type { ContactItem } from 'shadowlist-utils/native';
import { request } from '../api/network';
import { generateContact } from '../fixtures/contacts';

type TravellerRoute = 'Sections' | 'Changes' | 'Collapsing';

export const useTravellersQuery = (route: TravellerRoute) =>
  useQuery({
    queryKey: ['travellers', route],
    queryFn: ({ signal }) =>
      request(
        () =>
          Array.from({ length: route === 'Sections' ? 400 : 80 }, (_, index) =>
            generateContact(index)
          ),
        signal
      ),
    staleTime: Infinity,
  });

export function useChangeTravellers(route: TravellerRoute) {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (first: number) =>
      request(() =>
        Array.from({ length: route === 'Sections' ? 1 : 2 }, (_, index) =>
          generateContact(first + index)
        )
      ),
    onSuccess: (created) =>
      queryClient.setQueryData<ContactItem[]>(
        ['travellers', route],
        (items) => {
          if (!items) return items;
          if (route === 'Sections') return [...items, ...created];
          LayoutAnimation.configureNext(LayoutAnimation.Presets.easeInEaseOut);
          const next = items.filter((_, index) => index !== 2);
          next.splice(4, 0, ...created);
          return next;
        }
      ),
  });
}
