import { memo, useState } from 'react';
import {
  View,
  Text,
  Pressable,
  ActivityIndicator,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { CheckIcon, ChevronIcon, CloseIcon, StopIcon } from '../icons';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import type { AssistantToolCall, AssistantToolStatus } from './types';

export interface AssistantToolCallCardProps {
  call: AssistantToolCall;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

const StatusIcon = ({ status }: { status: AssistantToolStatus }) => {
  const { colors } = useTheme();
  switch (status) {
    case 'running':
      return <ActivityIndicator size="small" color={colors.secondaryLabel} />;
    case 'done':
      return <CheckIcon size={16} color={colors.green} strokeWidth={2} />;
    case 'stopped':
      return <StopIcon size={12} color={colors.tertiaryLabel} />;
    default:
      return <CloseIcon size={16} color={colors.red} strokeWidth={2} />;
  }
};

const outputOf = (call: AssistantToolCall) =>
  call.status === 'done' || call.status === 'failed' ? call.output : undefined;

/*
 * Memoized on the call object: the stream replaces only the call that changed, so finished
 * calls above a running one never re-render.
 */
export const AssistantToolCallCard = memo(
  ({ call, labels, style }: AssistantToolCallCardProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
    const [expanded, setExpanded] = useState(false);
    const output = outputOf(call);
    const statusLabel = l.toolStatus(call.status);

    return (
      <View style={[styles.card, style]}>
        <Pressable
          onPress={() => setExpanded((current) => !current)}
          accessibilityRole="button"
          accessibilityLabel={`${call.name}, ${statusLabel}`}
          accessibilityState={{ expanded }}
          style={({ pressed }) => [styles.header, pressed && styles.pressed]}
        >
          <View style={styles.statusIcon}>
            <StatusIcon status={call.status} />
          </View>
          <View style={styles.titleColumn}>
            <Text style={styles.name} numberOfLines={1}>
              {call.name}
            </Text>
            <Text style={styles.summary} numberOfLines={1}>
              {output || statusLabel}
            </Text>
          </View>
          <ChevronIcon
            direction={expanded ? 'down' : 'right'}
            size={14}
            color={theme.colors.tertiaryLabel}
            strokeWidth={1.8}
          />
        </Pressable>

        {expanded ? (
          <View style={styles.body}>
            <Text style={styles.sectionLabel}>{l.toolInput}</Text>
            <Text style={styles.mono} selectable>
              {call.input}
            </Text>
            {output ? (
              <>
                <Text style={styles.sectionLabel}>{l.toolOutput}</Text>
                <Text style={styles.mono} selectable>
                  {output}
                </Text>
              </>
            ) : null}
          </View>
        ) : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    card: {
      backgroundColor: theme.colors.elevated,
      borderRadius: theme.radius.md,
      overflow: 'hidden',
    },
    header: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.md,
      paddingHorizontal: theme.spacing.md,
      paddingVertical: 10,
    },
    statusIcon: {
      width: 20,
      alignItems: 'center',
    },
    titleColumn: {
      flex: 1,
    },
    name: {
      color: theme.colors.label,
      fontFamily: theme.fonts.mono,
      fontSize: theme.fontSize.footnote,
      fontWeight: theme.fontWeight.semibold,
    },
    summary: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.caption,
      marginTop: 1,
    },
    body: {
      gap: theme.spacing.xs,
      paddingHorizontal: theme.spacing.md,
      paddingBottom: theme.spacing.md,
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: theme.colors.separator,
      paddingTop: theme.spacing.sm,
    },
    sectionLabel: {
      color: theme.colors.tertiaryLabel,
      ...theme.typography.caption,
      fontWeight: theme.fontWeight.semibold,
      textTransform: 'uppercase',
    },
    mono: {
      color: theme.colors.label,
      fontFamily: theme.fonts.mono,
      fontSize: theme.fontSize.caption,
      lineHeight: 17,
    },
    pressed: {
      opacity: 0.6,
    },
  })
);
