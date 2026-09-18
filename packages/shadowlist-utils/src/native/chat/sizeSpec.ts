import type { ElementSizeSpec } from 'shadowlist';
import type { Theme } from '../theme';
import type { ChatMessage } from './types';

/*
 * A transcription of ChatBubble's styles; a spec that disagrees with its row predicts a
 * confidently wrong height, so edit both together.
 *
 *   container     paddingHorizontal spacing.md, paddingVertical spacing.xxs
 *   avatar        beside the column, not inside it, so it narrows nothing
 *   bubbleColumn  maxWidth 75% of the container's content box
 *   sender        typography.caption line + spacing.xxs margin, on messages from others
 *   bubble        paddingHorizontal BUBBLE_PADDING_HORIZONTAL, paddingVertical spacing.sm
 *   text          typography.body
 *   caption       typography.caption line + spacing.xs margin, inside the bubble
 *   failedStatus  typography.caption line + spacing.xs margin, under a failed bubble
 *
 * 'sending' only dims the bubble, so a send settling to 'sent' never changes the height.
 */
export const BUBBLE_WIDTH_FRACTION = 0.75;
export const BUBBLE_PADDING_HORIZONTAL = 14;

export interface ChatMessageSizeSpecOptions {
  // Set when the bubble renders a `caption`.
  caption?: boolean;
}

// Image messages get their height from the images, so they are measured natively.
export function getChatMessageSizeSpec(
  message: ChatMessage,
  theme: Theme,
  { caption = false }: ChatMessageSizeSpecOptions = {}
): ElementSizeSpec | null {
  if (
    !message.text ||
    (message.images !== undefined && message.images.length > 0)
  ) {
    return null;
  }
  const { spacing, typography } = theme;
  const senderHeight = typography.caption.lineHeight + spacing.xxs;
  const captionHeight = typography.caption.lineHeight + spacing.xs;
  const failedHeight = typography.caption.lineHeight + spacing.xs;

  return {
    text: message.text,
    fontSize: typography.body.fontSize,
    lineHeight: typography.body.lineHeight,
    letterSpacing: typography.body.letterSpacing,
    widthFraction: BUBBLE_WIDTH_FRACTION,
    // 0.75 x (W - 2 x container padding) - 2 x bubble padding
    insetWidth:
      BUBBLE_WIDTH_FRACTION * 2 * spacing.md + 2 * BUBBLE_PADDING_HORIZONTAL,
    insetHeight:
      2 * spacing.xxs +
      2 * spacing.sm +
      (message.isOwn ? 0 : senderHeight) +
      (caption ? captionHeight : 0) +
      (message.status === 'failed' ? failedHeight : 0),
  };
}
