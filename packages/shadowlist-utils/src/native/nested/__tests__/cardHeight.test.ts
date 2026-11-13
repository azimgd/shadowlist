import type { Theme } from '../../theme';
import { getCardHeight } from '../cardHeight';

// The tokens a card's height reads, at the values of the included themes.
const theme = {
  spacing: { sm: 8 },
  typography: { subhead: { lineHeight: 20 } },
} as unknown as Theme;

describe('getCardHeight', () => {
  it('holds the image, the gap and two lines of the title', () => {
    expect(getCardHeight(theme, 1)).toBe(220 + 8 + 2 * 20);
  });

  it('grows with the text size', () => {
    expect(getCardHeight(theme, 1.5)).toBe(220 + 8 + 2 * 30);
  });
});
