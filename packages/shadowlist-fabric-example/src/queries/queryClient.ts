import { AppState } from 'react-native';
import { QueryClient, focusManager } from '@tanstack/react-query';

export const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      // Cached lists render instantly when a screen is revisited, then refresh in the background.
      staleTime: 30_000,
    },
  },
});

/*
 * React Native has no window focus. Treat the app returning to the foreground as focus, so
 * stale queries on mounted screens refetch when the user comes back.
 */
focusManager.setEventListener((handleFocus) => {
  const subscription = AppState.addEventListener('change', (status) =>
    handleFocus(status === 'active')
  );
  return () => subscription.remove();
});
