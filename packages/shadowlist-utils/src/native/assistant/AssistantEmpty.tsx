import { memo } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { colors, typography, spacing, radius, fontWeight } from '../theme';
import { Sparkle } from '../icons';

export interface AssistantEmptyProps {
  title?: string;
  subtitle?: string;
  suggestions: { title: string; prompt: string }[];
  // Called with the suggestion's prompt; send it as if it were typed.
  onSelect: (prompt: string) => void;
}

// New-conversation state: the assistant mark, a greeting and a grid of starter prompts.
export const AssistantEmpty = memo(
  ({
    title = 'How can I help?',
    subtitle = 'Replies stream in token by token.',
    suggestions,
    onSelect,
  }: AssistantEmptyProps) => {
    return (
      <View style={styles.container}>
        <View style={styles.mark}>
          <Sparkle size={28} color={colors.label} />
        </View>
        <Text style={styles.title}>{title}</Text>
        <Text style={styles.subtitle}>{subtitle}</Text>

        <View style={styles.grid}>
          {suggestions.map((suggestion) => (
            <Pressable
              key={suggestion.prompt}
              onPress={() => onSelect(suggestion.prompt)}
              accessibilityRole="button"
              accessibilityHint={suggestion.prompt}
              style={({ pressed }) => [
                styles.card,
                pressed && styles.cardPressed,
              ]}
            >
              <Text style={styles.cardTitle}>{suggestion.title}</Text>
              <Text style={styles.cardPrompt} numberOfLines={2}>
                {suggestion.prompt}
              </Text>
            </Pressable>
          ))}
        </View>
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    alignItems: 'center',
    paddingHorizontal: spacing.lg,
    paddingTop: 48,
    paddingBottom: spacing.xxl,
  },
  mark: {
    width: 56,
    height: 56,
    borderRadius: radius.pill,
    backgroundColor: colors.accent,
    alignItems: 'center',
    justifyContent: 'center',
    marginBottom: spacing.lg,
  },
  title: {
    color: colors.label,
    ...typography.title2,
    textAlign: 'center',
  },
  subtitle: {
    color: colors.secondaryLabel,
    ...typography.subhead,
    textAlign: 'center',
    marginTop: spacing.xs,
  },
  grid: {
    alignSelf: 'stretch',
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: spacing.sm,
    marginTop: spacing.xxl,
  },
  card: {
    flexGrow: 1,
    flexBasis: '46%',
    padding: spacing.md,
    borderRadius: radius.md,
    backgroundColor: colors.elevated,
  },
  cardPressed: {
    backgroundColor: colors.elevated2,
  },
  cardTitle: {
    color: colors.label,
    ...typography.subhead,
    fontWeight: fontWeight.semibold,
  },
  cardPrompt: {
    color: colors.secondaryLabel,
    ...typography.footnote,
    marginTop: spacing.xxs,
  },
});
