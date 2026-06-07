import type { TextStyle } from 'react-native';

/*
 * iOS (dark mode) design tokens. Colors follow Apple's semantic system palette
 * so the templates read like a stock iOS app. `accent` is the single app tint —
 * change this one value to re-brand every control at once (e.g. Apple-default
 * blue '#0A84FF' vs. an orange '#FF9F0A').
 */
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

/*
 * iOS type ramp. fontFamily is intentionally omitted so React Native falls back
 * to San Francisco on iOS. Sizes/weights/tracking follow Apple's text styles.
 */
export const typography: Record<string, TextStyle> = {
  largeTitle: {
    fontSize: 34,
    fontWeight: '700',
    lineHeight: 41,
    letterSpacing: 0.37,
  },
  title2: {
    fontSize: 22,
    fontWeight: '700',
    lineHeight: 28,
    letterSpacing: 0.35,
  },
  title3: {
    fontSize: 20,
    fontWeight: '600',
    lineHeight: 25,
    letterSpacing: 0.38,
  },
  headline: {
    fontSize: 17,
    fontWeight: '600',
    lineHeight: 22,
    letterSpacing: -0.43,
  },
  body: {
    fontSize: 17,
    fontWeight: '400',
    lineHeight: 22,
    letterSpacing: -0.43,
  },
  callout: {
    fontSize: 16,
    fontWeight: '400',
    lineHeight: 21,
    letterSpacing: -0.31,
  },
  subhead: {
    fontSize: 15,
    fontWeight: '400',
    lineHeight: 20,
    letterSpacing: -0.24,
  },
  footnote: {
    fontSize: 13,
    fontWeight: '400',
    lineHeight: 18,
    letterSpacing: -0.08,
  },
  caption: {
    fontSize: 12,
    fontWeight: '400',
    lineHeight: 16,
    letterSpacing: 0,
  },
};

export const radius = {
  sm: 8,
  md: 12,
  lg: 16,
  pill: 999,
} as const;

// React Native's system font token — San Francisco on iOS, Roboto on Android.
// Mirrors the web `FONT_FAMILY` export so shared code can read the same key.
export const FONT_FAMILY = 'System';

// Leading inset that aligns separators / section content with the text column
// (avatar 40 + gutter 12 + leading padding 16).
export const ROW_INSET = 68;

// Single bundle of the design tokens, handy for spreading or theming.
export const theme = {
  colors,
  typography,
  radius,
  ROW_INSET,
  FONT_FAMILY,
} as const;
