import { Platform } from 'react-native';

const fontSize = {
  caption: 12,
  footnote: 13,
  subhead: 15,
  callout: 16,
  body: 17,
  title3: 20,
  title2: 22,
  largeTitle: 34,
} as const;

const fontWeight = {
  regular: '400',
  semibold: '600',
  bold: '700',
} as const;

// fontFamily is omitted so React Native falls back to the system font.
const typography = {
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
} as const;

const spacing = {
  xxs: 2,
  xs: 4,
  sm: 8,
  md: 12,
  lg: 16,
  xl: 20,
  xxl: 24,
} as const;

const radius = {
  sm: 8,
  md: 12,
  lg: 16,
  xl: 20,
  pill: 999,
} as const;

// Menlo ships on every iOS version. On Android, monospace maps to a system mono font.
const fonts = {
  mono: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
} as const;

export const baseTokens = {
  typography,
  fontSize,
  fontWeight,
  spacing,
  radius,
  fonts,
  rowInset: 68,
} as const;
