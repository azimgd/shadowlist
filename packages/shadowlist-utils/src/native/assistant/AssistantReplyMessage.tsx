import { memo, useEffect, useState } from 'react';
import { View, Text, Pressable, Linking, StyleSheet } from 'react-native';
import {
  colors,
  typography,
  spacing,
  radius,
  fontSize,
  fontWeight,
} from '../theme';
import { ArrowUp, Check, Chevron, Copy, Retry, Share, Sparkle } from '../icons';
import { AssistantActionButton } from './AssistantActionButton';
import { AssistantMarkdown } from './AssistantMarkdown';
import { AssistantTypingIndicator } from './AssistantTypingIndicator';
import { AssistantThinking } from './AssistantThinking';
import { AssistantToolCallCard } from './AssistantToolCallCard';
import { useStreamingTurn, type AssistantStreamStore } from './stream';
import {
  emptyTurn,
  type AssistantFeedback,
  type AssistantReply,
  type AssistantSource,
  type AssistantTurn,
} from './data';

export interface AssistantReplyMessageProps {
  message: AssistantReply;
  // Where this reply's in-flight turn lives while it streams.
  store: AssistantStreamStore;
  // Follow-up suggestions appear only under the newest reply.
  isLatest?: boolean;
  /*
   * Some reply in the conversation is streaming. Only one may at a time, so everything that
   * would start another (regenerate, retry) is disabled until it finishes.
   */
  busy?: boolean;
  onCopy?: (text: string) => void;
  onCopyCode?: (code: string) => void;
  onShare?: (text: string) => void;
  onRegenerate?: (messageId: string) => void;
  onRetry?: (messageId: string) => void;
  onSelectVariant?: (messageId: string, variantIndex: number) => void;
  onFeedback?: (messageId: string, feedback: AssistantFeedback) => void;
  onFollowUp?: (prompt: string) => void;
}

// How long the copy action reads "Copied" before reverting.
const COPIED_RESET_MS = 1500;

// Rendered only if variantIndex ever points past the variants array.
const MISSING_TURN: AssistantTurn = { ...emptyTurn(), status: 'done' };

const openSource = (url: string) => {
  Linking.openURL(url).catch(() => {});
};

const SourceChip = memo(
  ({ source, index }: { source: AssistantSource; index: number }) => (
    <Pressable
      onPress={() => openSource(source.url)}
      accessibilityRole="link"
      accessibilityLabel={`Source ${index + 1}: ${source.title}`}
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
          {source.domain}
        </Text>
      </View>
    </Pressable>
  )
);

/*
 * One assistant reply, Claude/ChatGPT style: full width, no bubble. Top to bottom it is
 * reasoning, tool calls, the Markdown answer with a trailing cursor, a stopped or failed
 * notice, sources, the action bar with a variant pager, and follow-up suggestions.
 *
 * While streaming, the turn comes from the store rather than from `message`, so the list
 * `data` never changes per token; only this row re-renders, once per flush.
 */
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
  }: AssistantReplyMessageProps) => {
    const live = useStreamingTurn(store, message.id);
    const turn = live ?? message.variants[message.variantIndex] ?? MISSING_TURN;
    const [copied, setCopied] = useState(false);

    useEffect(() => {
      if (!copied) return;
      const id = setTimeout(() => setCopied(false), COPIED_RESET_MS);
      return () => clearTimeout(id);
    }, [copied]);

    const streaming = turn.status === 'streaming';
    const thinkingActive = streaming && !turn.thinkingMs && !!turn.thinking;
    // Nothing has arrived yet: no reasoning, no tools, no text.
    const waiting =
      streaming &&
      !turn.thinking &&
      !turn.content &&
      turn.toolCalls.length === 0;
    const variantCount = message.variants.length;

    return (
      <View style={styles.container}>
        <View style={styles.header}>
          <View style={styles.avatar}>
            <Sparkle size={14} color={colors.label} />
          </View>
          <Text style={styles.name}>Assistant</Text>
          <Text style={styles.model}>{message.model}</Text>
        </View>

        {turn.thinking ? (
          <AssistantThinking
            thinking={turn.thinking}
            thinkingMs={turn.thinkingMs}
            active={thinkingActive}
          />
        ) : null}

        {turn.toolCalls.map((call) => (
          <AssistantToolCallCard key={call.id} call={call} />
        ))}

        {waiting ? <AssistantTypingIndicator /> : null}

        {turn.content ? (
          <AssistantMarkdown
            text={turn.content}
            streaming={streaming}
            onCopyCode={onCopyCode}
          />
        ) : null}

        {turn.status === 'stopped' ? (
          <Text style={styles.notice}>Response stopped</Text>
        ) : null}

        {turn.status === 'error' ? (
          <View style={styles.error}>
            <Text style={styles.errorText}>{turn.error}</Text>
            <Pressable
              onPress={() => onRetry?.(message.id)}
              disabled={busy}
              accessibilityRole="button"
              accessibilityLabel="Retry"
              accessibilityState={{ disabled: busy }}
              style={({ pressed }) => [
                styles.retryButton,
                (pressed || busy) && styles.pressed,
              ]}
            >
              <Retry size={16} color={colors.label} strokeWidth={1.6} />
              <Text style={styles.retryText}>Retry</Text>
            </Pressable>
          </View>
        ) : null}

        {turn.sources.length > 0 ? (
          <View style={styles.sources}>
            {turn.sources.map((source, index) => (
              <SourceChip key={source.id} source={source} index={index} />
            ))}
          </View>
        ) : null}

        {/*
         * Kept MOUNTED while streaming, merely invisible. Mounting the action row when the
         * reply finishes adds a 32pt row to the bottom of the newest message at the exact
         * moment the stream settles, which shifts the text the reader is on. Hidden it
         * still takes touches and still reads out, so both are turned off explicitly.
         */}
        <View
          style={[styles.actions, streaming && styles.actionsHidden]}
          pointerEvents={streaming ? 'none' : 'auto'}
          accessibilityElementsHidden={streaming}
          importantForAccessibility={streaming ? 'no-hide-descendants' : 'auto'}
        >
          {turn.content ? (
            <AssistantActionButton
              label={copied ? 'Copied' : 'Copy'}
              onPress={() => {
                onCopy?.(turn.content);
                setCopied(true);
              }}
            >
              {copied ? (
                <Check size={16} color={colors.secondaryLabel} />
              ) : (
                <Copy size={18} color={colors.secondaryLabel} />
              )}
            </AssistantActionButton>
          ) : null}
          <AssistantActionButton
            label="Regenerate"
            disabled={busy}
            onPress={() => onRegenerate?.(message.id)}
          >
            <Retry size={18} color={colors.secondaryLabel} />
          </AssistantActionButton>
          <AssistantActionButton
            label="Good response"
            selected={message.feedback === 'good'}
            onPress={() =>
              onFeedback?.(
                message.id,
                message.feedback === 'good' ? null : 'good'
              )
            }
          >
            <ArrowUp
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
            label="Bad response"
            selected={message.feedback === 'bad'}
            onPress={() =>
              onFeedback?.(
                message.id,
                message.feedback === 'bad' ? null : 'bad'
              )
            }
          >
            <View style={styles.flipped}>
              <ArrowUp
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
              label="Share"
              onPress={() => onShare?.(turn.content)}
            >
              <Share size={18} color={colors.secondaryLabel} />
            </AssistantActionButton>
          ) : null}

          {variantCount > 1 ? (
            <View style={styles.pager}>
              <AssistantActionButton
                label="Previous version"
                disabled={message.variantIndex === 0}
                onPress={() =>
                  onSelectVariant?.(message.id, message.variantIndex - 1)
                }
              >
                <Chevron
                  direction="left"
                  size={14}
                  color={colors.secondaryLabel}
                />
              </AssistantActionButton>
              <Text style={styles.pagerText}>
                {`${message.variantIndex + 1} / ${variantCount}`}
              </Text>
              <AssistantActionButton
                label="Next version"
                disabled={message.variantIndex === variantCount - 1}
                onPress={() =>
                  onSelectVariant?.(message.id, message.variantIndex + 1)
                }
              >
                <Chevron
                  direction="right"
                  size={14}
                  color={colors.secondaryLabel}
                />
              </AssistantActionButton>
            </View>
          ) : null}
        </View>

        {/*
         * Follow-ups are not given the same treatment: they exist only once the turn is
         * 'done', so they appear after the pin has already settled, and reserving space for
         * suggestions that may never come would leave a gap under every reply.
         */}
        {isLatest && turn.status === 'done' && turn.followUps.length > 0 ? (
          <View style={styles.followUps}>
            {turn.followUps.map((prompt) => (
              <Pressable
                key={prompt}
                onPress={() => onFollowUp?.(prompt)}
                accessibilityRole="button"
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

const styles = StyleSheet.create({
  container: {
    paddingHorizontal: spacing.lg,
    paddingVertical: spacing.md,
    gap: spacing.md,
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.sm,
  },
  avatar: {
    width: 24,
    height: 24,
    borderRadius: radius.pill,
    backgroundColor: colors.accent,
    alignItems: 'center',
    justifyContent: 'center',
  },
  name: {
    color: colors.label,
    ...typography.subhead,
    fontWeight: fontWeight.semibold,
  },
  model: {
    color: colors.tertiaryLabel,
    ...typography.footnote,
  },
  notice: {
    color: colors.tertiaryLabel,
    ...typography.footnote,
    fontStyle: 'italic',
  },
  error: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.md,
    padding: spacing.md,
    borderRadius: radius.md,
    backgroundColor: colors.redSoft,
  },
  errorText: {
    flex: 1,
    color: colors.red,
    ...typography.subhead,
  },
  retryButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.xs,
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.xs + 2,
    borderRadius: radius.sm,
    backgroundColor: colors.fill,
  },
  retryText: {
    color: colors.label,
    ...typography.footnote,
    fontWeight: fontWeight.semibold,
  },
  sources: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: spacing.sm,
  },
  source: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.sm,
    maxWidth: 200,
    paddingVertical: spacing.xs + 2,
    paddingLeft: spacing.xs + 2,
    paddingRight: spacing.md,
    borderRadius: radius.md,
    backgroundColor: colors.elevated,
  },
  sourceIndex: {
    width: 20,
    height: 20,
    borderRadius: radius.pill,
    backgroundColor: colors.fill,
    alignItems: 'center',
    justifyContent: 'center',
  },
  sourceIndexText: {
    color: colors.label,
    fontSize: fontSize.caption,
    fontWeight: fontWeight.semibold,
  },
  sourceMeta: {
    flexShrink: 1,
  },
  sourceTitle: {
    color: colors.label,
    ...typography.caption,
    fontWeight: fontWeight.semibold,
  },
  sourceDomain: {
    color: colors.secondaryLabel,
    ...typography.caption,
  },
  actions: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.xs,
    marginLeft: -spacing.sm,
  },
  // Invisible but still occupying its row, so finishing a reply changes no height.
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
    color: colors.secondaryLabel,
    ...typography.footnote,
    minWidth: 36,
    textAlign: 'center',
  },
  followUps: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: spacing.sm,
  },
  followUp: {
    backgroundColor: colors.accentSoft,
    borderRadius: radius.sm,
    paddingHorizontal: 14,
    paddingVertical: spacing.sm,
  },
  followUpPressed: {
    opacity: 0.6,
  },
  followUpText: {
    color: colors.accent,
    ...typography.footnote,
    fontWeight: fontWeight.semibold,
  },
  pressed: {
    opacity: 0.6,
  },
});
