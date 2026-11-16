import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import type { PollData } from 'shadowlist-utils/native';
import { request } from '../api/network';
import { destinationPoll } from '../fixtures/poll';

const pollKey = ['poll'] as const;

export const usePollQuery = () =>
  useQuery({
    queryKey: pollKey,
    queryFn: ({ signal }) => request(() => destinationPoll, signal),
    staleTime: Infinity,
  });

export function useVote() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (id: string) => request(() => id),
    onSuccess: (id) =>
      queryClient.setQueryData<PollData>(pollKey, (poll) => {
        if (!poll || poll.selectedId === id) return poll;
        return {
          ...poll,
          selectedId: id,
          options: poll.options.map((option) => ({
            ...option,
            votes:
              option.votes +
              Number(option.id === id) -
              Number(option.id === poll.selectedId),
          })),
        };
      }),
  });
}
