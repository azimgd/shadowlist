import type { CSSProperties } from 'react';

// iOS (dark mode) design tokens. Change `accent` to re-brand every control.
export const colors = {
  background: '#000000',
  elevated: '#1C1C1E',
  elevated2: '#2C2C2E',

  label: '#FFFFFF',
  secondaryLabel: 'rgba(235,235,245,0.6)',
  tertiaryLabel: 'rgba(235,235,245,0.3)',

  separator: 'rgba(84,84,88,0.6)',
  fill: 'rgba(118,118,128,0.24)',

  accent: '#0A84FF',
  accentSoft: 'rgba(10,132,255,0.16)',

  blue: '#0A84FF',
  green: '#30D158',
  red: '#FF453B',
} as const;

// San Francisco where available, falling back to each platform's UI font.
export const FONT_FAMILY =
  '-apple-system, BlinkMacSystemFont, "SF Pro Text", "Segoe UI", Roboto, system-ui, sans-serif';

// iOS type ramp. lineHeight/letterSpacing are px strings so React DOM keeps units.
export const typography: Record<string, CSSProperties> = {
  largeTitle: { fontSize: 34, fontWeight: 700, lineHeight: '41px', letterSpacing: '0.37px' },
  title2: { fontSize: 22, fontWeight: 700, lineHeight: '28px', letterSpacing: '0.35px' },
  title3: { fontSize: 20, fontWeight: 600, lineHeight: '25px', letterSpacing: '0.38px' },
  headline: { fontSize: 17, fontWeight: 600, lineHeight: '22px', letterSpacing: '-0.43px' },
  body: { fontSize: 17, fontWeight: 400, lineHeight: '22px', letterSpacing: '-0.43px' },
  callout: { fontSize: 16, fontWeight: 400, lineHeight: '21px', letterSpacing: '-0.31px' },
  subhead: { fontSize: 15, fontWeight: 400, lineHeight: '20px', letterSpacing: '-0.24px' },
  footnote: { fontSize: 13, fontWeight: 400, lineHeight: '18px', letterSpacing: '-0.08px' },
  caption: { fontSize: 12, fontWeight: 400, lineHeight: '16px' },
};

export const radius = {
  sm: 8,
  md: 12,
  lg: 16,
  pill: 999,
} as const;

// Leading inset aligning separators / text with the content column.
export const ROW_INSET = 68;

export const theme = { colors, typography, radius, ROW_INSET, FONT_FAMILY } as const;
