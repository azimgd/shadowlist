import { useQuery } from '@tanstack/react-query';
import { fetchFileTree } from '../api/files';

export const useFileTreeQuery = () =>
  useQuery({
    queryKey: ['files', 'tree'],
    queryFn: ({ signal }) => fetchFileTree(signal),
  });
