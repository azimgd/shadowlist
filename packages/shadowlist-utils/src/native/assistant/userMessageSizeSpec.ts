import type { ItemSizeSpec } from 'shadowlist';
import type { Theme } from '../theme';
import type { AssistantPrompt } from './types';

/*
 * The size spec below copies these numbers and the bubble styles of AssistantUserMessage. Keep
 * them in step or the predicted height is wrong. assistantUserMessageSizeSpec in the core's
 * utils/ChatLayout.hpp is the same rule, and vectors.test holds the two equal.
 */
export const BUBBLE_WIDTH_FRACTION = 0.8;
export const BUBBLE_PADDING_HORIZONTAL = 14;
export const BUBBLE_PADDING_VERTICAL = 10;

/*
 * Describes a plain text user message so native can size it before it renders. A sent
 * message then never starts at a guessed height and pushes the reply under it. Messages
 * with attachments are measured natively instead.
 */
export function getUserMessageSizeSpec(
  message: AssistantPrompt,
  theme: Theme
): ItemSizeSpec | null {
  if (!message.text || (message.attachments?.length ?? 0) > 0) return null;

  const { body } = theme.typography;
  return {
    text: message.text,
    fontSize: body.fontSize,
    lineHeight: body.lineHeight,
    letterSpacing: body.letterSpacing,
    widthFraction: BUBBLE_WIDTH_FRACTION,
    // A share of the container's inner width, minus the bubble's own padding.
    insetWidth:
      BUBBLE_WIDTH_FRACTION * theme.spacing.lg * 2 +
      BUBBLE_PADDING_HORIZONTAL * 2,
    insetHeight: theme.spacing.sm * 2 + BUBBLE_PADDING_VERTICAL * 2,
  };
}
