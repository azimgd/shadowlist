import { memo } from 'react';
import {
  View,
  Text,
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { SparkleIcon } from '../icons';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import type { AssistantSuggestion } from './types';

export interface AssistantEmptyProps {
  suggestions?: readonly AssistantSuggestion[];
  onSelectSuggestion?: (suggestion: AssistantSuggestion) => void;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

/*
 * Drawn over the list instead of as ListEmptyComponent. The list's content size never
 * counts the empty template, so with no rows Android clips it out of view.
 */
export const AssistantEmpty = memo(
  ({ suggestions, onSelectSuggestion, labels, style }: AssistantEmptyProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);

    return (
      <View style={[styles.container, style]}>
        <View style={styles.mark}>
          {/* The accent's own foreground on the accent disc, as in the reply header: label was black on blue. */}
          <SparkleIcon size={28} color={theme.colors.onAccent} />
        </View>
        <Text style={styles.title} accessibilityRole="header">
          {l.emptyTitle}
        </Text>
        {l.emptySubtitle ? (
          <Text style={styles.subtitle}>{l.emptySubtitle}</Text>
        ) : null}

        {suggestions && suggestions.length > 0 ? (
          <View style={styles.grid}>
            {suggestions.map((suggestion) => (
              <Pressable
                key={suggestion.prompt}
                onPress={() => onSelectSuggestion?.(suggestion)}
                accessibilityRole="button"
                accessibilityLabel={suggestion.title}
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
        ) : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      alignItems: 'center',
      paddingHorizontal: theme.spacing.lg,
      paddingTop: 48,
      paddingBottom: theme.spacing.xxl,
    },
    mark: {
      width: 56,
      height: 56,
      borderRadius: theme.radius.pill,
      backgroundColor: theme.colors.accent,
      alignItems: 'center',
      justifyContent: 'center',
      marginBottom: theme.spacing.lg,
    },
    title: {
      color: theme.colors.label,
      ...theme.typography.title2,
      textAlign: 'center',
    },
    subtitle: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.subhead,
      textAlign: 'center',
      marginTop: theme.spacing.xs,
    },
    grid: {
      alignSelf: 'stretch',
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: theme.spacing.sm,
      marginTop: theme.spacing.xxl,
    },
    card: {
      flexGrow: 1,
      flexBasis: '46%',
      padding: theme.spacing.md,
      borderRadius: theme.radius.md,
      backgroundColor: theme.colors.elevated,
    },
    cardPressed: {
      backgroundColor: theme.colors.elevated2,
    },
    cardTitle: {
      color: theme.colors.label,
      ...theme.typography.subhead,
      fontWeight: theme.fontWeight.semibold,
    },
    cardPrompt: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.footnote,
      marginTop: theme.spacing.xxs,
    },
  })
);
