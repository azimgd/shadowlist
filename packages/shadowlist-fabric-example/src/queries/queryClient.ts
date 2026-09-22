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
 * React Native has no window focus, so treat the app coming back to the foreground as focus.
 * Stale queries on mounted screens then refetch when the user returns.
 */
focusManager.setEventListener((handleFocus) => {
  const subscription = AppState.addEventListener('change', (status) =>
    handleFocus(status === 'active')
  );
  return () => subscription.remove();
});
