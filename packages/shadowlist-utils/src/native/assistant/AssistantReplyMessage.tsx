import { memo, useEffect, useState } from 'react';
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
import {
  ArrowUpIcon,
  CheckIcon,
  ChevronIcon,
  CopyIcon,
  RetryIcon,
  ShareIcon,
  SparkleIcon,
} from '../icons';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import { openUrl } from './openUrl';
import { AssistantActionButton } from './AssistantActionButton';
import { AssistantMarkdown } from './AssistantMarkdown';
import { AssistantTypingIndicator } from './AssistantTypingIndicator';
import { AssistantThinking } from './AssistantThinking';
import { AssistantToolCallCard } from './AssistantToolCallCard';
import { useStreamingTurn, type AssistantStreamStore } from './stream';
import type {
  AssistantToolCall,
  AssistantFeedback,
  AssistantReply,
  AssistantSource,
  AssistantTurn,
} from './types';

export interface AssistantReplyMessageProps {
  message: AssistantReply;
  store: AssistantStreamStore;
  // Show follow up suggestions only under the newest reply.
  isLatest?: boolean;
  // Disables Regenerate and Retry while another reply is streaming.
  busy?: boolean;
  onCopy?: (text: string) => void;
  onCopyCode?: (code: string) => void;
  onShare?: (text: string) => void;
  onRegenerate?: (messageId: string) => void;
  onRetry?: (messageId: string) => void;
  onSelectVariant?: (messageId: string, variantIndex: number) => void;
  // Passing undefined clears the reader's feedback.
  onFeedback?: (
    messageId: string,
    feedback: AssistantFeedback | undefined
  ) => void;
  onFollowUp?: (prompt: string) => void;
  // Opens sources and Markdown links. By default only web and mailto links open.
  onOpenLink?: (url: string) => void;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

const COPIED_RESET_MS = 1500;

const MISSING_TURN: AssistantTurn = { status: 'done', content: '' };

type Step =
  | { kind: 'text'; text: string; at: number }
  | { kind: 'calls'; calls: AssistantToolCall[] };

/*
 * Splits the reply text where each tool call was made and puts the calls between the pieces.
 * A call with no saved position, from an older transcript, goes first. Blank pieces are
 * dropped, since the card already leaves a gap.
 *
 * Calls made back to back, with no text between them, are one step: they draw as one grouped
 * card with a row per call, the way iOS lists a run of related rows in an inset group, rather
 * than as a stack of separate cards. A model that writes, fills, reads and formats in one breath
 * produced four floating cards taller than the answer they led to.
 */
function stepsOf(content: string, calls: readonly AssistantToolCall[]): Step[] {
  const steps: Step[] = [];
  let cursor = 0;
  const pushText = (end: number) => {
    const text = content.slice(cursor, end);
    if (text.trim() !== '')
      steps.push({ kind: 'text', text: text.trim(), at: cursor });
    cursor = end;
  };
  const ordered = calls
    .map((call, index) => ({
      call,
      index,
      at: Math.min(call.at ?? 0, content.length),
    }))
    .sort((a, b) => a.at - b.at || a.index - b.index);
  for (const { call, at } of ordered) {
    if (at > cursor) pushText(at);
    const last = steps[steps.length - 1];
    if (last !== undefined && last.kind === 'calls') last.calls.push(call);
    else steps.push({ kind: 'calls', calls: [call] });
  }
  pushText(content.length);
  return steps;
}

const domainOf = (source: AssistantSource) =>
  source.domain ?? source.url.replace(/^[a-z]+:\/\//i, '').split('/')[0];

const SourceChip = memo(
  ({
    source,
    index,
    onOpenLink,
    labels,
  }: {
    source: AssistantSource;
    index: number;
    onOpenLink: (url: string) => void;
    labels: AssistantLabels;
  }) => {
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

export const AssistantReplyMessage = memo(
  ({
    message,
    store,
    isLatest = false,
    busy = false,
    onCopy,
    onCopyCode,
    onShare,
    onRegenerate,
    onRetry,
    onSelectVariant,
    onFeedback,
    onFollowUp,
    onOpenLink = openUrl,
    labels,
    style,
  }: AssistantReplyMessageProps) => {
    const theme = useTheme();
    const { colors } = theme;
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
    const live = useStreamingTurn(store, message.id);
    const turn = live ?? message.variants[message.variantIndex] ?? MISSING_TURN;
    const [copied, setCopied] = useState(false);

    useEffect(() => {
      if (!copied) return;
      const id = setTimeout(() => setCopied(false), COPIED_RESET_MS);
      return () => clearTimeout(id);
    }, [copied]);

    const streaming = turn.status === 'streaming';
    const toolCalls = turn.toolCalls ?? [];
    const thinkingActive = streaming && !turn.thinkingMs && !!turn.thinking;
    const waiting =
      streaming && !turn.thinking && !turn.content && toolCalls.length === 0;
    const sources = turn.status === 'done' ? (turn.sources ?? []) : [];
    const followUps = turn.status === 'done' ? (turn.followUps ?? []) : [];
    const variantCount = message.variants.length;

    return (
      <View style={[styles.container, style]}>
        <View style={styles.header}>
          <View style={styles.avatar}>
            {/* On the accent disc, so the accent's own foreground: label was black on blue in light mode. */}
            <SparkleIcon size={14} color={colors.onAccent} />
          </View>
          <Text style={styles.name}>{l.assistantName}</Text>
          {message.model ? (
            <Text style={styles.model}>{message.model}</Text>
          ) : null}
        </View>

        {turn.thinking ? (
          <AssistantThinking
            thinking={turn.thinking}
            thinkingMs={turn.thinkingMs}
            active={thinkingActive}
            labels={l}
          />
        ) : null}

        {waiting ? <AssistantTypingIndicator labels={l} /> : null}

        {/*
         * Text and tool calls in the order they happened. Only the last piece of text still
         * grows, so everything above it stays put while the reply streams.
         */}
        {stepsOf(turn.content, toolCalls).map((step, index, steps) =>
          step.kind === 'calls' ? (
            <View key={step.calls[0]!.id} style={styles.toolGroup}>
              {step.calls.map((call, row) => (
                <AssistantToolCallCard
                  key={call.id}
                  call={call}
                  labels={l}
                  grouped
                  separated={row > 0}
                />
              ))}
            </View>
          ) : (
            <AssistantMarkdown
              key={`text-${step.at}`}
              text={step.text}
              streaming={streaming && index === steps.length - 1}
              onCopyCode={onCopyCode}
              onOpenLink={onOpenLink}
              labels={l}
            />
          )
        )}

        {turn.status === 'stopped' ? (
          <Text style={styles.notice}>{l.responseStopped}</Text>
        ) : null}

        {turn.status === 'failed' ? (
          <View style={styles.error}>
            <Text style={styles.errorText}>
              {turn.error || l.responseFailed}
            </Text>
            <Pressable
              onPress={() => onRetry?.(message.id)}
              disabled={busy}
              accessibilityRole="button"
              accessibilityLabel={l.retry}
              accessibilityState={{ disabled: busy }}
              style={({ pressed }) => [
                styles.retryButton,
                (pressed || busy) && styles.pressed,
              ]}
            >
              <RetryIcon size={16} color={colors.label} strokeWidth={1.6} />
              <Text style={styles.retryText}>{l.retry}</Text>
            </Pressable>
          </View>
        ) : null}

        {sources.length > 0 ? (
          <View style={styles.sources}>
            {sources.map((source, index) => (
              <SourceChip
                key={source.id}
                source={source}
                index={index}
                onOpenLink={onOpenLink}
                labels={l}
              />
            ))}
          </View>
        ) : null}

        {/*
         * Stays mounted but invisible while streaming. Mounting it at the end would add a
         * 32pt row and shift the text the reader is on. Hidden views still take touches and
         * get read out, so both are turned off.
         */}
        <View
          style={[styles.actions, streaming && styles.actionsHidden]}
          pointerEvents={streaming ? 'none' : 'auto'}
          accessibilityElementsHidden={streaming}
          importantForAccessibility={streaming ? 'no-hide-descendants' : 'auto'}
        >
          {turn.content ? (
            <AssistantActionButton
              label={copied ? l.copied : l.copy}
              onPress={() => {
                onCopy?.(turn.content);
                setCopied(true);
              }}
            >
              {copied ? (
                <CheckIcon size={16} color={colors.secondaryLabel} />
              ) : (
                <CopyIcon size={18} color={colors.secondaryLabel} />
              )}
            </AssistantActionButton>
          ) : null}
          <AssistantActionButton
            label={l.regenerate}
            disabled={busy}
            onPress={() => onRegenerate?.(message.id)}
          >
            <RetryIcon size={18} color={colors.secondaryLabel} />
          </AssistantActionButton>
          <AssistantActionButton
            label={l.goodResponse}
            selected={message.feedback === 'good'}
            onPress={() =>
              onFeedback?.(
                message.id,
                message.feedback === 'good' ? undefined : 'good'
              )
            }
          >
            <ArrowUpIcon
              size={18}
              strokeWidth={1.8}
              color={
                message.feedback === 'good'
                  ? colors.accent
                  : colors.secondaryLabel
              }
            />
          </AssistantActionButton>
          <AssistantActionButton
            label={l.badResponse}
            selected={message.feedback === 'bad'}
            onPress={() =>
              onFeedback?.(
                message.id,
                message.feedback === 'bad' ? undefined : 'bad'
              )
            }
          >
            <View style={styles.flipped}>
              <ArrowUpIcon
                size={18}
                strokeWidth={1.8}
                color={
                  message.feedback === 'bad'
                    ? colors.red
                    : colors.secondaryLabel
                }
              />
            </View>
          </AssistantActionButton>
          {turn.content ? (
            <AssistantActionButton
              label={l.share}
              onPress={() => onShare?.(turn.content)}
            >
              <ShareIcon size={18} color={colors.secondaryLabel} />
            </AssistantActionButton>
          ) : null}

          {variantCount > 1 ? (
            <View style={styles.pager}>
              <AssistantActionButton
                label={l.previousVersion}
                disabled={message.variantIndex === 0}
                onPress={() =>
                  onSelectVariant?.(message.id, message.variantIndex - 1)
                }
              >
                <ChevronIcon
                  direction="left"
                  size={14}
                  color={colors.secondaryLabel}
                />
              </AssistantActionButton>
              <Text style={styles.pagerText}>
                {l.versionPosition(message.variantIndex + 1, variantCount)}
              </Text>
              <AssistantActionButton
                label={l.nextVersion}
                disabled={message.variantIndex === variantCount - 1}
                onPress={() =>
                  onSelectVariant?.(message.id, message.variantIndex + 1)
                }
              >
                <ChevronIcon
                  direction="right"
                  size={14}
                  color={colors.secondaryLabel}
                />
              </AssistantActionButton>
            </View>
          ) : null}
        </View>

        {/*
         * Follow ups only exist once the turn is done, after scrolling has settled. Reserving
         * space for them would leave a gap under every reply.
         */}
        {isLatest && followUps.length > 0 ? (
          <View style={styles.followUps}>
            {followUps.map((prompt) => (
              <Pressable
                key={prompt}
                onPress={() => onFollowUp?.(prompt)}
                accessibilityRole="button"
                accessibilityLabel={prompt}
                style={({ pressed }) => [
                  styles.followUp,
                  pressed && styles.followUpPressed,
                ]}
              >
                <Text style={styles.followUpText}>{prompt}</Text>
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
      paddingHorizontal: theme.spacing.lg,
      paddingVertical: theme.spacing.md,
      gap: theme.spacing.md,
    },
    toolGroup: {
      backgroundColor: theme.colors.elevated,
      borderRadius: theme.radius.md,
      overflow: 'hidden',
    },
    header: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.sm,
    },
    avatar: {
      width: 24,
      height: 24,
      borderRadius: theme.radius.pill,
      backgroundColor: theme.colors.accent,
      alignItems: 'center',
      justifyContent: 'center',
    },
    name: {
      color: theme.colors.label,
      ...theme.typography.subhead,
      fontWeight: theme.fontWeight.semibold,
    },
    model: {
      color: theme.colors.tertiaryLabel,
      ...theme.typography.footnote,
    },
    notice: {
      color: theme.colors.tertiaryLabel,
      ...theme.typography.footnote,
      fontStyle: 'italic',
    },
    error: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.md,
      padding: theme.spacing.md,
      borderRadius: theme.radius.md,
      backgroundColor: theme.colors.redSoft,
    },
    errorText: {
      flex: 1,
      color: theme.colors.red,
      ...theme.typography.subhead,
    },
    retryButton: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.xs,
      paddingHorizontal: theme.spacing.md,
      paddingVertical: theme.spacing.xs + 2,
      borderRadius: theme.radius.sm,
      backgroundColor: theme.colors.fill,
    },
    retryText: {
      color: theme.colors.label,
      ...theme.typography.footnote,
      fontWeight: theme.fontWeight.semibold,
    },
    sources: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: theme.spacing.sm,
    },
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
    actions: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.xs,
      marginLeft: -theme.spacing.sm,
    },
    // Invisible but still takes up its row, so finishing a reply changes no height.
    actionsHidden: {
      opacity: 0,
    },
    flipped: {
      transform: [{ rotate: '180deg' }],
    },
    pager: {
      flexDirection: 'row',
      alignItems: 'center',
      marginLeft: 'auto',
    },
    pagerText: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.footnote,
      minWidth: 36,
      textAlign: 'center',
    },
    followUps: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: theme.spacing.sm,
    },
    followUp: {
      backgroundColor: theme.colors.accentSoft,
      borderRadius: theme.radius.sm,
      paddingHorizontal: 14,
      paddingVertical: theme.spacing.sm,
    },
    followUpPressed: {
      opacity: 0.6,
    },
    followUpText: {
      color: theme.colors.accent,
      ...theme.typography.footnote,
      fontWeight: theme.fontWeight.semibold,
    },
    pressed: {
      opacity: 0.6,
    },
  })
);
