import { View, Text, Pressable, StyleSheet } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import {
  DrawerContentScrollView,
  DrawerItemList,
  type DrawerContentComponentProps,
} from '@react-navigation/drawer';
import { createStyles } from 'shadowlist-utils/native';

export type ThemePreference = 'system' | 'light' | 'dark';

const OPTIONS: { value: ThemePreference; label: string }[] = [
  { value: 'system', label: 'System' },
  { value: 'light', label: 'Light' },
  { value: 'dark', label: 'Dark' },
];

interface ThemeSwitcherProps {
  value: ThemePreference;
  onChange: (value: ThemePreference) => void;
}

export const ThemeSwitcher = ({ value, onChange }: ThemeSwitcherProps) => {
  const styles = useStyles();
  return (
    <View style={styles.switcher}>
      <Text style={styles.caption} nativeID="appearance-label">
        Appearance
      </Text>
      <View
        style={styles.segments}
        accessibilityRole="radiogroup"
        accessibilityLabelledBy="appearance-label"
        accessibilityLabel="Appearance"
      >
        {OPTIONS.map((option) => {
          const selected = option.value === value;
          return (
            <Pressable
              key={option.value}
              onPress={() => onChange(option.value)}
              accessibilityRole="radio"
              accessibilityState={{ checked: selected, selected }}
              accessibilityLabel={option.label}
              style={({ pressed }) => [
                styles.segment,
                selected && styles.segmentSelected,
                pressed && styles.pressed,
              ]}
            >
              <Text
                style={[
                  styles.segmentText,
                  selected && styles.segmentTextSelected,
                ]}
              >
                {option.label}
              </Text>
            </Pressable>
          );
        })}
      </View>
    </View>
  );
};

export const DrawerContent = ({
  themePreference,
  onThemePreferenceChange,
  ...props
}: DrawerContentComponentProps & {
  themePreference: ThemePreference;
  onThemePreferenceChange: (value: ThemePreference) => void;
}) => {
  const styles = useStyles();
  const insets = useSafeAreaInsets();
  return (
    <View style={styles.container}>
      <DrawerContentScrollView {...props}>
        <DrawerItemList {...props} />
      </DrawerContentScrollView>
      <View style={{ paddingBottom: insets.bottom }}>
        <ThemeSwitcher
          value={themePreference}
          onChange={onThemePreferenceChange}
        />
      </View>
    </View>
  );
};

const useStyles = createStyles(({ colors, typography, spacing, radius }) =>
  StyleSheet.create({
    container: {
      flex: 1,
    },
    switcher: {
      paddingHorizontal: spacing.lg,
      paddingVertical: spacing.md,
      gap: spacing.sm,
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: colors.separator,
    },
    caption: {
      color: colors.secondaryLabel,
      ...typography.footnote,
    },
    segments: {
      flexDirection: 'row',
      padding: spacing.xxs,
      borderRadius: radius.sm + spacing.xxs,
      backgroundColor: colors.fill,
    },
    segment: {
      flex: 1,
      alignItems: 'center',
      paddingVertical: 6,
      borderRadius: radius.sm,
    },
    segmentSelected: {
      backgroundColor: colors.accentSoft,
    },
    pressed: {
      opacity: 0.6,
    },
    segmentText: {
      color: colors.secondaryLabel,
      ...typography.subhead,
    },
    segmentTextSelected: {
      color: colors.accent,
      fontWeight: '600',
    },
  })
);
