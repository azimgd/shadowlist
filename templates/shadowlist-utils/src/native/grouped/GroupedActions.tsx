import { useMemo, type ReactNode } from 'react';
import type { ColorValue, StyleProp, ViewStyle } from 'react-native';
import {
  ItemActionsView,
  type ContextMenu,
  type SwipeActionsConfiguration,
} from 'shadowlist';
import { useTheme } from '../theme';

export interface GroupedActionsProps {
  leadingSwipeActions?: SwipeActionsConfiguration<undefined> | null;
  trailingSwipeActions?: SwipeActionsConfiguration<undefined> | null;
  contextMenu?: ContextMenu<undefined> | null;
  children: ReactNode;
  style?: StyleProp<ViewStyle>;
}

/*
 * A destructive action without its own color takes the theme's red.
 */
function themedSwipeActions(
  configuration: SwipeActionsConfiguration<undefined> | null | undefined,
  red: ColorValue
): SwipeActionsConfiguration<undefined> | null | undefined {
  if (!configuration) return configuration;
  return {
    ...configuration,
    actions: configuration.actions.map((action) =>
      action.style === 'destructive' && action.backgroundColor === undefined
        ? { ...action, backgroundColor: red }
        : action
    ),
  };
}

/*
 * A card row with the swipe actions and menu of a list row. A swipe reveals the actions on the
 * phone and a right click shows the menu on the Mac. The row inside needs its own background.
 */
export const GroupedActions = ({
  leadingSwipeActions,
  trailingSwipeActions,
  contextMenu,
  children,
  style,
}: GroupedActionsProps) => {
  const { colors } = useTheme();
  const leading = useMemo(
    () => themedSwipeActions(leadingSwipeActions, colors.red),
    [leadingSwipeActions, colors.red]
  );
  const trailing = useMemo(
    () => themedSwipeActions(trailingSwipeActions, colors.red),
    [trailingSwipeActions, colors.red]
  );
  return (
    <ItemActionsView
      leadingSwipeActions={leading}
      trailingSwipeActions={trailing}
      contextMenu={contextMenu}
      style={style}
    >
      {children}
    </ItemActionsView>
  );
};
