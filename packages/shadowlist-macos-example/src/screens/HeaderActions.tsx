import { useLayoutEffect, useRef } from 'react';
import {
  useEntryKey,
  useToolbarMenus,
  type MenuGroups,
} from '../navigation/ToolbarMenu';

/*
 * The same hooks the iOS and Android example screens call. On macOS the actions land in the
 * toolbar's More menu instead of a navigation bar. The SF Symbol names are kept for parity
 * with the iOS menu but are not drawn.
 */
export interface HeaderAction {
  label: string;
  symbol: string;
  onPress: () => void;
  destructive?: boolean;
  checked?: boolean;
}

export interface HeaderActionHandlers {
  onPrepend: () => void;
  onAppend: () => void;
  onScrollToRandom: () => void;
  prependLabel?: string;
  appendLabel?: string;
}

type ActionGroups = HeaderAction[][];

export function useHeaderActions(
  handlers: HeaderActionHandlers,
  extraGroups: ActionGroups = []
) {
  const groups: ActionGroups = [
    [
      {
        label: handlers.prependLabel ?? 'Add to Top',
        symbol: 'arrow.up.to.line',
        onPress: handlers.onPrepend,
      },
      {
        label: handlers.appendLabel ?? 'Add to Bottom',
        symbol: 'arrow.down.to.line',
        onPress: handlers.onAppend,
      },
      {
        label: 'Jump to Random Item',
        symbol: 'scope',
        onPress: handlers.onScrollToRandom,
      },
    ],
    ...extraGroups,
  ];
  useHeaderMenu(groups);
}

/*
 * Publishes the menu only when a label or a check changes. Each item calls through a ref.
 * The newest handlers run without republishing on every render.
 */
export function useHeaderMenu(groups: ActionGroups) {
  const entryKey = useEntryKey();
  const { setMenu } = useToolbarMenus();
  const ref = useRef(groups);
  ref.current = groups;

  const signature = groups
    .map((group) =>
      group.map((a) => `${a.label}|${a.checked}|${a.destructive}`).join(',')
    )
    .join(';');

  useLayoutEffect(() => {
    const menu: MenuGroups = ref.current.map((group, groupIndex) =>
      group.map((action, index) => ({
        label: action.label,
        destructive: action.destructive,
        checked: action.checked,
        onPress: () => ref.current[groupIndex]?.[index]?.onPress(),
      }))
    );
    setMenu(entryKey, menu);
  }, [entryKey, setMenu, signature]);

  useLayoutEffect(() => () => setMenu(entryKey, null), [entryKey, setMenu]);
}
