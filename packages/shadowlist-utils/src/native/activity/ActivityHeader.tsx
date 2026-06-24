import { memo } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { colors, typography, radius, spacing, fontWeight } from '../theme';

export interface ActivityHeaderAction {
  label: string;
  onPress: () => void;
}

export interface ActivityHeaderProps {
  title?: string;
  subtitle?: string;
  actions?: ActivityHeaderAction[];
}

const TintedButton = ({
  label,
  onPress,
}: {
  label: string;
  onPress: () => void;
}) => (
  <Pressable
    onPress={onPress}
    style={({ pressed }) => [styles.action, pressed && styles.actionPressed]}
  >
    <Text style={styles.actionText}>{label}</Text>
  </Pressable>
);

/*
 * Large-title header with an optional row of tinted action buttons. Pass
 * `actions` to surface imperative controls (scroll, edit, ...) above a list.
 */
export const ActivityHeader = memo(
  ({ title = 'Activity', subtitle, actions }: ActivityHeaderProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.title}>{title}</Text>
        {subtitle ? <Text style={styles.subtitle}>{subtitle}</Text> : null}

        {actions && actions.length > 0 ? (
          <View style={styles.row}>
            {actions.map((action) => (
              <TintedButton
                key={action.label}
                label={action.label}
                onPress={action.onPress}
              />
            ))}
          </View>
        ) : null}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    backgroundColor: colors.background,
    paddingHorizontal: spacing.lg,
    paddingTop: spacing.xs,
    paddingBottom: spacing.md,
  },
  title: {
    color: colors.label,
    ...typography.largeTitle,
  },
  subtitle: {
    color: colors.secondaryLabel,
    ...typography.subhead,
    marginTop: spacing.xxs,
  },
  row: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: spacing.sm,
    marginTop: 14,
  },
  action: {
    backgroundColor: colors.accentSoft,
    borderRadius: radius.sm,
    paddingHorizontal: 14,
    paddingVertical: spacing.sm,
  },
  actionPressed: {
    opacity: 0.6,
  },
  actionText: {
    color: colors.accent,
    ...typography.footnote,
    fontWeight: fontWeight.semibold,
  },
});
