import {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useState,
  type ReactNode,
} from 'react';

export interface MenuAction {
  label: string;
  onPress: () => void;
  destructive?: boolean;
  checked?: boolean;
}

export type MenuGroups = MenuAction[][];

interface ToolbarMenus {
  menus: ReadonlyMap<number, MenuGroups>;
  setMenu: (entryKey: number, groups: MenuGroups | null) => void;
}

const ToolbarMenusContext = createContext<ToolbarMenus | null>(null);
const EntryKeyContext = createContext<number>(-1);

/*
 * Holds the More menu of every screen in the stack, keyed by its stack entry. A covered
 * screen keeps its menu. The toolbar shows the one for the screen on top.
 */
export function ToolbarMenuProvider({ children }: { children: ReactNode }) {
  const [menus, setMenus] = useState<ReadonlyMap<number, MenuGroups>>(
    () => new Map()
  );
  const setMenu = useCallback(
    (entryKey: number, groups: MenuGroups | null) =>
      setMenus((previous) => {
        const next = new Map(previous);
        if (groups) next.set(entryKey, groups);
        else next.delete(entryKey);
        return next;
      }),
    []
  );
  const value = useMemo(() => ({ menus, setMenu }), [menus, setMenu]);
  return (
    <ToolbarMenusContext.Provider value={value}>
      {children}
    </ToolbarMenusContext.Provider>
  );
}

export function useToolbarMenus(): ToolbarMenus {
  const menus = useContext(ToolbarMenusContext);
  if (!menus) {
    throw new Error('useToolbarMenus needs a ToolbarMenuProvider');
  }
  return menus;
}

export const EntryKeyProvider = EntryKeyContext.Provider;

export function useEntryKey(): number {
  return useContext(EntryKeyContext);
}
