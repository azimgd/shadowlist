import { useLayoutEffect, useRef, useState } from 'react';
import type { ReactNode, RefObject } from 'react';
import {
  Modal,
  Platform,
  Pressable,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { useNavigation } from '@react-navigation/native';
import type {
  NativeStackHeaderItem,
  NativeStackHeaderItemMenuAction,
  NativeStackHeaderItemMenuSubmenu,
  NativeStackNavigationProp,
} from '@react-navigation/native-stack';
import type { ParamListBase } from '@react-navigation/native';
import {
  CheckIcon,
  ChevronIcon,
  createStyles,
  useTheme,
} from 'shadowlist-utils/native';
import { EllipsisIcon, ViewfinderIcon } from './icons';
import { DEBUG } from './launchSettings';

export interface HeaderAction {
  label: string;
  symbol: SymbolName;
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

type SymbolName = Extract<
  NonNullable<NativeStackHeaderItemMenuAction['icon']>,
  { type: 'sfSymbol' }
>['name'];

/*
 * A More menu: native UIMenu on iOS, a look-alike popup on Android.
 * With -SLDebug 1 the first three actions are also direct buttons for the trace runner.
 */
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

export function useHeaderMenu(groups: ActionGroups) {
  const navigation = useNavigation<NativeStackNavigationProp<ParamListBase>>();
  const { colors } = useTheme();
  const ref = useRef(groups);
  ref.current = groups;

  const signature = groups
    .map((group) =>
      group
        .map((a) => `${a.label}|${a.symbol}|${a.checked}|${a.destructive}`)
        .join(',')
    )
    .join(';');

  useLayoutEffect(() => {
    if (Platform.OS === 'ios') {
      navigation.setOptions({
        unstable_headerRightItems: () => iosItems(ref),
      });
    } else {
      navigation.setOptions({
        headerRight: () => <AndroidHeaderActions groups={ref} />,
      });
    }
  }, [navigation, signature, colors]);
}

const call =
  (groups: RefObject<ActionGroups>, groupIndex: number, index: number) => () =>
    groups.current[groupIndex]?.[index]?.onPress();

function iosItems(groups: RefObject<ActionGroups>): NativeStackHeaderItem[] {
  const toMenuAction = (
    action: HeaderAction,
    groupIndex: number,
    index: number
  ): NativeStackHeaderItemMenuAction => ({
    type: 'action',
    label: action.label,
    icon: { type: 'sfSymbol', name: action.symbol },
    destructive: action.destructive,
    state:
      action.checked === undefined ? undefined : action.checked ? 'on' : 'off',
    onPress: call(groups, groupIndex, index),
  });
  const sections: NativeStackHeaderItemMenuSubmenu[] = groups.current.map(
    (group, groupIndex) => ({
      type: 'submenu',
      label: '',
      inline: true,
      items: group.map((action, index) =>
        toMenuAction(action, groupIndex, index)
      ),
    })
  );
  const menu: NativeStackHeaderItem = {
    type: 'menu',
    label: 'More',
    accessibilityLabel: 'More',
    icon: { type: 'sfSymbol', name: 'ellipsis' },
    menu: { items: sections },
  };
  if (!DEBUG) return [menu];
  const first = groups.current[0] ?? [];
  const buttons: NativeStackHeaderItem[] = first.map((action, index) => ({
    type: 'button',
    label: action.label,
    accessibilityLabel: action.label,
    icon: { type: 'sfSymbol', name: action.symbol },
    onPress: call(groups, 0, index),
  }));
  return [...buttons, menu];
}

const HeaderButton = ({
  onPress,
  label,
  children,
}: {
  onPress: () => void;
  label: string;
  children: ReactNode;
}) => (
  <Pressable
    onPress={onPress}
    accessibilityRole="button"
    accessibilityLabel={label}
    hitSlop={6}
    style={({ pressed }) => [
      staticStyles.button,
      pressed && staticStyles.pressed,
    ]}
  >
    {children}
  </Pressable>
);

const AndroidHeaderActions = ({
  groups,
}: {
  groups: RefObject<ActionGroups>;
}) => {
  const { colors } = useTheme();
  const [open, setOpen] = useState(false);
  const first = groups.current[0] ?? [];
  return (
    <View style={staticStyles.bar}>
      {DEBUG ? (
        <>
          <HeaderButton
            label={first[0]?.label ?? ''}
            onPress={call(groups, 0, 0)}
          >
            <ChevronIcon
              direction="up"
              color={colors.accent}
              size={22}
              strokeWidth={2.25}
            />
          </HeaderButton>
          <HeaderButton
            label={first[1]?.label ?? ''}
            onPress={call(groups, 0, 1)}
          >
            <ChevronIcon
              direction="down"
              color={colors.accent}
              size={22}
              strokeWidth={2.25}
            />
          </HeaderButton>
          <HeaderButton
            label={first[2]?.label ?? ''}
            onPress={call(groups, 0, 2)}
          >
            <ViewfinderIcon color={colors.accent} size={22} strokeWidth={2} />
          </HeaderButton>
        </>
      ) : null}
      <HeaderButton label="More" onPress={() => setOpen(true)}>
        <EllipsisIcon color={colors.accent} size={22} />
      </HeaderButton>
      <PopupMenu
        visible={open}
        groups={groups.current}
        onClose={() => setOpen(false)}
      />
    </View>
  );
};

const PopupMenu = ({
  visible,
  groups,
  onClose,
}: {
  visible: boolean;
  groups: ActionGroups;
  onClose: () => void;
}) => {
  const styles = useMenuStyles();
  const { colors } = useTheme();
  const insets = useSafeAreaInsets();
  return (
    <Modal
      visible={visible}
      transparent
      animationType="fade"
      onRequestClose={onClose}
      statusBarTranslucent
    >
      <Pressable
        style={styles.backdrop}
        onPress={onClose}
        accessibilityLabel="Close menu"
      />
      <View
        style={[styles.card, { top: insets.top + 52 }]}
        accessibilityRole="menu"
      >
        {groups.map((group, groupIndex) => (
          <View
            key={groupIndex}
            style={groupIndex > 0 ? styles.groupGap : undefined}
          >
            {group.map((action, index) => (
              <Pressable
                key={action.label}
                accessibilityRole="menuitem"
                accessibilityState={
                  action.checked === undefined
                    ? undefined
                    : { checked: action.checked }
                }
                onPress={() => {
                  onClose();
                  action.onPress();
                }}
                style={({ pressed }) => [
                  styles.row,
                  index > 0 && styles.rowSeparator,
                  pressed && styles.rowPressed,
                ]}
              >
                <View style={styles.check}>
                  {action.checked ? (
                    <CheckIcon size={16} color={colors.label} />
                  ) : null}
                </View>
                <Text
                  style={[
                    styles.label,
                    action.destructive && styles.destructive,
                  ]}
                >
                  {action.label}
                </Text>
              </Pressable>
            ))}
          </View>
        ))}
      </View>
    </Modal>
  );
};

const staticStyles = StyleSheet.create({
  bar: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  button: {
    width: 40,
    height: 40,
    alignItems: 'center',
    justifyContent: 'center',
  },
  pressed: {
    opacity: 0.35,
  },
});

const useMenuStyles = createStyles(({ colors, typography, spacing }) =>
  StyleSheet.create({
    backdrop: {
      ...StyleSheet.absoluteFill,
      backgroundColor: 'rgba(0,0,0,0.12)',
    },
    card: {
      position: 'absolute',
      right: spacing.sm,
      minWidth: 250,
      maxWidth: 320,
      borderRadius: 14,
      overflow: 'hidden',
      backgroundColor: colors.elevated,
      elevation: 12,
    },
    groupGap: {
      borderTopWidth: spacing.sm,
      borderTopColor: colors.separator,
    },
    row: {
      flexDirection: 'row',
      alignItems: 'center',
      minHeight: 48,
      paddingRight: spacing.lg,
      paddingVertical: spacing.sm,
    },
    rowSeparator: {
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: colors.separator,
    },
    rowPressed: {
      backgroundColor: colors.fill,
    },
    check: {
      width: 36,
      alignItems: 'center',
    },
    label: {
      flex: 1,
      color: colors.label,
      ...typography.body,
    },
    destructive: {
      color: colors.red,
    },
  })
);
