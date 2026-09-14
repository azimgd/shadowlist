import { memo, useState } from 'react';
import {
  View,
  Text,
  Pressable,
  ActivityIndicator,
  StyleSheet,
} from 'react-native';
import {
  colors,
  typography,
  spacing,
  radius,
  fontSize,
  fontWeight,
  MONO_FONT_FAMILY,
} from '../theme';
import { Check, Chevron, Close, Stop } from '../icons';
import type { AssistantToolInvocation, AssistantToolStatus } from './data';

export interface AssistantToolCallCardProps {
  call: AssistantToolInvocation;
}

const STATUS_LABEL: Record<AssistantToolStatus, string> = {
  running: 'Running',
  done: 'Done',
  error: 'Failed',
  stopped: 'Stopped',
};

const StatusIcon = ({ status }: { status: AssistantToolStatus }) => {
  switch (status) {
    case 'running':
      return <ActivityIndicator size="small" color={colors.secondaryLabel} />;
    case 'done':
      return <Check size={16} color={colors.green} strokeWidth={2} />;
    // The reader stopped it: neutral grey, not the red an actual failure gets.
    case 'stopped':
      return <Stop size={12} color={colors.tertiaryLabel} />;
    default:
      return <Close size={16} color={colors.red} strokeWidth={2} />;
  }
};

/*
 * One tool invocation: status, name and a one-line result, expanding to the raw input
 * and output. Memoized on the call object -- the stream replaces only the call that
 * changed, so finished calls above a running one never re-render.
 */
export const AssistantToolCallCard = memo(
  ({ call }: AssistantToolCallCardProps) => {
    const [expanded, setExpanded] = useState(false);

    return (
      <View style={styles.card}>
        <Pressable
          onPress={() => setExpanded((current) => !current)}
          accessibilityRole="button"
          accessibilityLabel={`${call.name}, ${STATUS_LABEL[call.status]}`}
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
              {call.output || STATUS_LABEL[call.status]}
            </Text>
          </View>
          <Chevron
            direction={expanded ? 'down' : 'right'}
            size={14}
            color={colors.tertiaryLabel}
            strokeWidth={1.8}
          />
        </Pressable>

        {expanded ? (
          <View style={styles.body}>
            <Text style={styles.sectionLabel}>Input</Text>
            <Text style={styles.mono} selectable>
              {call.input}
            </Text>
            {call.output ? (
              <>
                <Text style={styles.sectionLabel}>Output</Text>
                <Text style={styles.mono} selectable>
                  {call.output}
                </Text>
              </>
            ) : null}
          </View>
        ) : null}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  card: {
    backgroundColor: colors.elevated,
    borderRadius: radius.md,
    overflow: 'hidden',
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.md,
    paddingHorizontal: spacing.md,
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
    color: colors.label,
    fontFamily: MONO_FONT_FAMILY,
    fontSize: fontSize.footnote,
    fontWeight: fontWeight.semibold,
  },
  summary: {
    color: colors.secondaryLabel,
    ...typography.caption,
    marginTop: 1,
  },
  body: {
    gap: spacing.xs,
    paddingHorizontal: spacing.md,
    paddingBottom: spacing.md,
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: colors.separator,
    paddingTop: spacing.sm,
  },
  sectionLabel: {
    color: colors.tertiaryLabel,
    ...typography.caption,
    fontWeight: fontWeight.semibold,
    textTransform: 'uppercase',
  },
  mono: {
    color: colors.label,
    fontFamily: MONO_FONT_FAMILY,
    fontSize: fontSize.caption,
    lineHeight: 17,
  },
  pressed: {
    opacity: 0.6,
  },
});
