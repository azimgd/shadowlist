import type { ReactNode } from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import {
  CheckIcon,
  ChevronIcon,
  useTheme,
  withEffect,
} from 'shadowlist-utils/native';
import { EllipsisIcon } from '@example/icons';
import type { MenuAction, MenuGroups } from './ToolbarMenu';
import { useHover } from './useHover';

export const TOOLBAR_HEIGHT = 38;

interface ToolbarButtonProps {
  label: string;
  selected?: boolean;
  onPress: () => void;
  children: ReactNode;
}

/*
 * A toolbar item takes AppKit's own pressed and rollover tints off the accent rather than an
 * opacity change. A system effect rides on the resolved color. It composes with a semantic
 * base and stays correct in Dark Mode and against any System accent the user picked.
 */
function ToolbarButton({
  label,
  selected = false,
  onPress,
  children,
}: ToolbarButtonProps) {
  const { colors } = useTheme();
  const [hovered, hover] = useHover();
  return (
    <Pressable
      onPress={onPress}
      accessibilityRole="button"
      accessibilityLabel={label}
      accessibilityState={{ selected }}
      {...hover}
      style={({ pressed }) => [
        styles.item,
        selected && { backgroundColor: colors.fill },
        hovered &&
          !pressed && {
            backgroundColor: withEffect(colors.accent, 'rollover'),
          },
        pressed && { backgroundColor: withEffect(colors.accent, 'pressed') },
      ]}
    >
      {children}
    </Pressable>
  );
}

interface ToolbarProps {
  title: string;
  backTitle?: string;
  onBack?: () => void;
  menuOpen: boolean;
  onPressMenu?: () => void;
}

export function Toolbar({
  title,
  backTitle,
  onBack,
  menuOpen,
  onPressMenu,
}: ToolbarProps) {
  const { colors } = useTheme();
  return (
    <View style={[styles.toolbar, { borderBottomColor: colors.separator }]}>
      {onBack ? (
        <ToolbarButton label={`Back to ${backTitle ?? ''}`} onPress={onBack}>
          <View style={styles.back}>
            <ChevronIcon direction="left" size={14} color={colors.label} />
            <Text style={[styles.label, { color: colors.label }]}>
              {backTitle}
            </Text>
          </View>
        </ToolbarButton>
      ) : null}
      <Text
        numberOfLines={1}
        accessibilityRole="header"
        style={[styles.title, { color: colors.label }]}
      >
        {title}
      </Text>
      <View style={styles.spacer} />
      {onPressMenu ? (
        <ToolbarButton label="More" selected={menuOpen} onPress={onPressMenu}>
          <EllipsisIcon size={16} color={colors.label} />
        </ToolbarButton>
      ) : null}
    </View>
  );
}

function MenuItem({
  action,
  onPress,
}: {
  action: MenuAction;
  onPress: () => void;
}) {
  const { colors } = useTheme();
  const [hovered, hover] = useHover();
  const color = hovered
    ? colors.onAccent
    : action.destructive
      ? colors.red
      : colors.label;
  return (
    <Pressable
      onPress={onPress}
      accessibilityRole="menuitem"
      accessibilityState={
        action.checked === undefined ? undefined : { checked: action.checked }
      }
      {...hover}
      style={[styles.menuItem, hovered && { backgroundColor: colors.accent }]}
    >
      <View style={styles.check}>
        {action.checked ? <CheckIcon size={12} color={color} /> : null}
      </View>
      <Text numberOfLines={1} style={[styles.menuLabel, { color }]}>
        {action.label}
      </Text>
    </Pressable>
  );
}

export function MoreMenu({
  groups,
  onClose,
}: {
  groups: MenuGroups;
  onClose: () => void;
}) {
  const { colors } = useTheme();
  return (
    <View style={StyleSheet.absoluteFill}>
      <Pressable
        style={StyleSheet.absoluteFill}
        onPress={onClose}
        accessibilityLabel="Close menu"
      />
      <View
        accessibilityRole="menu"
        style={[
          styles.menu,
          { backgroundColor: colors.elevated, borderColor: colors.separator },
        ]}
      >
        {groups.map((group, groupIndex) => (
          <View
            key={groupIndex}
            style={
              groupIndex > 0
                ? [styles.menuGroup, { borderTopColor: colors.separator }]
                : undefined
            }
          >
            {group.map((action) => (
              <MenuItem
                key={action.label}
                action={action}
                onPress={() => {
                  onClose();
                  action.onPress();
                }}
              />
            ))}
          </View>
        ))}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  toolbar: {
    height: TOOLBAR_HEIGHT,
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
    paddingHorizontal: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
  },
  item: {
    minWidth: 28,
    height: 24,
    paddingHorizontal: 6,
    borderRadius: 5,
    alignItems: 'center',
    justifyContent: 'center',
  },
  back: { flexDirection: 'row', alignItems: 'center', gap: 2 },
  label: { fontSize: 12 },
  title: { fontSize: 13, fontWeight: '600', marginLeft: 4, flexShrink: 1 },
  spacer: { flex: 1 },
  menu: {
    position: 'absolute',
    top: TOOLBAR_HEIGHT - 4,
    right: 8,
    minWidth: 220,
    maxWidth: 320,
    paddingVertical: 5,
    paddingHorizontal: 5,
    borderRadius: 8,
    borderWidth: StyleSheet.hairlineWidth,
    shadowColor: '#000',
    shadowOpacity: 0.2,
    shadowRadius: 12,
    shadowOffset: { width: 0, height: 4 },
  },
  menuGroup: {
    marginTop: 5,
    paddingTop: 5,
    borderTopWidth: StyleSheet.hairlineWidth,
  },
  menuItem: {
    height: 22,
    flexDirection: 'row',
    alignItems: 'center',
    paddingRight: 12,
    borderRadius: 4,
  },
  check: { width: 20, alignItems: 'center' },
  menuLabel: { fontSize: 13 },
});
