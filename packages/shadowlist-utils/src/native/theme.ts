import type { TextStyle } from 'react-native';

/*
 * iOS (dark mode) design tokens. Colors follow Apple's semantic system palette
 * so the templates read like a stock iOS app. `accent` is the single app tint;
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

// Type-ramp sizes (pt) and weights, named after the iOS text styles below. Use these for
// any raw fontSize / fontWeight so the templates share one type scale.
export const fontSize = {
  caption: 12,
  footnote: 13,
  subhead: 15,
  callout: 16,
  body: 17,
  title3: 20,
  title2: 22,
  largeTitle: 34,
} as const;

export const fontWeight = {
  regular: '400',
  semibold: '600',
  bold: '700',
} as const;

/*
 * iOS type ramp. fontFamily is intentionally omitted so React Native falls back
 * to San Francisco on iOS. Sizes/weights/tracking follow Apple's text styles.
 */
export const typography: Record<string, TextStyle> = {
  largeTitle: {
    fontSize: fontSize.largeTitle,
    fontWeight: fontWeight.bold,
    lineHeight: 41,
    letterSpacing: 0.37,
  },
  title2: {
    fontSize: fontSize.title2,
    fontWeight: fontWeight.bold,
    lineHeight: 28,
    letterSpacing: 0.35,
  },
  title3: {
    fontSize: fontSize.title3,
    fontWeight: fontWeight.semibold,
    lineHeight: 25,
    letterSpacing: 0.38,
  },
  headline: {
    fontSize: fontSize.body,
    fontWeight: fontWeight.semibold,
    lineHeight: 22,
    letterSpacing: -0.43,
  },
  body: {
    fontSize: fontSize.body,
    fontWeight: fontWeight.regular,
    lineHeight: 22,
    letterSpacing: -0.43,
  },
  callout: {
    fontSize: fontSize.callout,
    fontWeight: fontWeight.regular,
    lineHeight: 21,
    letterSpacing: -0.31,
  },
  subhead: {
    fontSize: fontSize.subhead,
    fontWeight: fontWeight.regular,
    lineHeight: 20,
    letterSpacing: -0.24,
  },
  footnote: {
    fontSize: fontSize.footnote,
    fontWeight: fontWeight.regular,
    lineHeight: 18,
    letterSpacing: -0.08,
  },
  caption: {
    fontSize: fontSize.caption,
    fontWeight: fontWeight.regular,
    lineHeight: 16,
    letterSpacing: 0,
  },
};

// Spacing scale (4-point grid) for padding, margin and gap. Use these instead of
// raw numbers so the templates share one rhythm; re-scale the whole UI from here.
export const spacing = {
  xxs: 2,
  xs: 4,
  sm: 8,
  md: 12,
  lg: 16,
  xl: 20,
  xxl: 24,
} as const;

export const radius = {
  sm: 8,
  md: 12,
  lg: 16,
  xl: 20,
  pill: 999,
} as const;

// React Native's system font token: San Francisco on iOS, Roboto on Android.
export const FONT_FAMILY = 'System';

// Leading inset that aligns separators / section content with the text column
// (avatar 40 + gutter 12 + leading padding 16).
export const ROW_INSET = 68;

// Single bundle of the design tokens, handy for spreading or theming.
export const theme = {
  colors,
  typography,
  fontSize,
  fontWeight,
  spacing,
  radius,
  ROW_INSET,
  FONT_FAMILY,
} as const;
