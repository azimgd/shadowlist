import { memo } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { createStyles } from '../theme';
import type { AssistantLabels } from './labels';
import type { AssistantSource } from './types';

export interface AssistantSourceChipProps {
  source: AssistantSource;
  index: number;
  onOpenLink: (url: string) => void;
  labels: AssistantLabels;
}

const domainOf = (source: AssistantSource) =>
  source.domain ?? source.url.replace(/^[a-z]+:\/\//i, '').split('/')[0];

export const AssistantSourceChip = memo(
  ({ source, index, onOpenLink, labels }: AssistantSourceChipProps) => {
    const styles = useStyles();
    return (
      <Pressable
        onPress={() => onOpenLink(source.url)}
        accessibilityRole="link"
        accessibilityLabel={labels.source(index + 1, source.title)}
        style={({ pressed }) => [styles.source, pressed && styles.pressed]}
      >
        <View style={styles.sourceIndex}>
          <Text style={styles.sourceIndexText}>{index + 1}</Text>
        </View>
        <View style={styles.sourceMeta}>
          <Text style={styles.sourceTitle} numberOfLines={1}>
            {source.title}
          </Text>
          <Text style={styles.sourceDomain} numberOfLines={1}>
            {domainOf(source)}
          </Text>
        </View>
      </Pressable>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    source: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.sm,
      maxWidth: 200,
      paddingVertical: theme.spacing.xs + 2,
      paddingLeft: theme.spacing.xs + 2,
      paddingRight: theme.spacing.md,
      borderRadius: theme.radius.md,
      backgroundColor: theme.colors.elevated,
    },
    sourceIndex: {
      width: 20,
      height: 20,
      borderRadius: theme.radius.pill,
      backgroundColor: theme.colors.fill,
      alignItems: 'center',
      justifyContent: 'center',
    },
    sourceIndexText: {
      color: theme.colors.label,
      fontSize: theme.fontSize.caption,
      fontWeight: theme.fontWeight.semibold,
    },
    sourceMeta: {
      flexShrink: 1,
    },
    sourceTitle: {
      color: theme.colors.label,
      ...theme.typography.caption,
      fontWeight: theme.fontWeight.semibold,
    },
    sourceDomain: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.caption,
    },
    pressed: {
      opacity: 0.6,
    },
  })
);
