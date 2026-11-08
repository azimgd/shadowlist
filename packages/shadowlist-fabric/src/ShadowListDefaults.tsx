import { createContext, useContext, type ReactNode } from 'react';
import type { ShadowListProps } from './types';

/*
 * Props every list under a ShadowListDefaultsProvider takes when it does not set them: an app
 * whose platform draws no scroll bars in lists sets them once instead of on every list. A list's
 * own prop wins.
 */
export type ShadowListDefaults = Pick<
  ShadowListProps<unknown>,
  'showsVerticalScrollIndicator' | 'showsHorizontalScrollIndicator'
>;

const ShadowListDefaultsContext = createContext<ShadowListDefaults>({});

export function ShadowListDefaultsProvider({
  value,
  children,
}: {
  value: ShadowListDefaults;
  children: ReactNode;
}) {
  return (
    <ShadowListDefaultsContext.Provider value={value}>
      {children}
    </ShadowListDefaultsContext.Provider>
  );
}

export function useShadowListDefaults(): ShadowListDefaults {
  return useContext(ShadowListDefaultsContext);
}
