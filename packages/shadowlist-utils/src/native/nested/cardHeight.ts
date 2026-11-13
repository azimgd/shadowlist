import type { Theme } from '../theme';

export const CARD_IMAGE_HEIGHT = 220;
export const CARD_TITLE_LINES = 2;

/*
 * How tall a nested card is: its image, the gap under it and the lines of its title at the
 * reader's text size. A row's list of cards is this tall. The UIKit and Android templates
 * follow the same rule.
 */
export function getCardHeight(theme: Theme, fontScale: number): number {
  return (
    CARD_IMAGE_HEIGHT +
    theme.spacing.sm +
    CARD_TITLE_LINES * theme.typography.subhead.lineHeight * fontScale
  );
}
