import {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from 'react';
import type { ExampleRoute, Route } from '../routes';

export interface StackEntry {
  key: number;
  route: Route;
}

/*
 * select starts a new stack, the way picking a sidebar item does.
 */
export interface Navigation {
  stack: StackEntry[];
  select: (name: 'Home' | ExampleRoute) => void;
  push: (route: Route) => void;
  goBack: () => void;
}

const NavigationContext = createContext<Navigation | null>(null);

/*
 * The detail pane's stack. The sidebar picks its root and a screen can push on top of it.
 * A Mac app has no swipe back. The toolbar shows a back button while the stack is deep.
 */
export function NavigationProvider({
  initial,
  children,
}: {
  initial: Route;
  children: ReactNode;
}) {
  const nextKey = useRef(1);
  const [stack, setStack] = useState<StackEntry[]>(() => [
    { key: 0, route: initial },
  ]);

  const select = useCallback((name: 'Home' | ExampleRoute) => {
    setStack((previous) =>
      previous.length === 1 && previous[0]?.route.name === name
        ? previous
        : [{ key: nextKey.current++, route: { name } }]
    );
  }, []);
  const push = useCallback((route: Route) => {
    setStack((previous) => [...previous, { key: nextKey.current++, route }]);
  }, []);
  const goBack = useCallback(() => {
    setStack((previous) =>
      previous.length > 1 ? previous.slice(0, -1) : previous
    );
  }, []);

  const value = useMemo(
    () => ({ stack, select, push, goBack }),
    [stack, select, push, goBack]
  );
  return (
    <NavigationContext.Provider value={value}>
      {children}
    </NavigationContext.Provider>
  );
}

export function useNavigation(): Navigation {
  const navigation = useContext(NavigationContext);
  if (!navigation) {
    throw new Error('useNavigation needs a NavigationProvider');
  }
  return navigation;
}
