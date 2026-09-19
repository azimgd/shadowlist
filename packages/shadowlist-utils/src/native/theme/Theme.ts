import type { TextStyle } from 'react-native';

export interface ThemeColors {
  background: string;
  elevated: string;
  elevated2: string;
  label: string;
  secondaryLabel: string;
  tertiaryLabel: string;
  separator: string;
  fill: string;
  accent: string;
  accentSoft: string;
  onAccent: string;
  blue: string;
  green: string;
  red: string;
  redSoft: string;
  avatarPalette: ReadonlyArray<string>;
}

export interface ThemeTextStyle {
  fontSize: number;
  fontWeight: NonNullable<TextStyle['fontWeight']>;
  lineHeight: number;
  letterSpacing: number;
}

export interface ThemeTypography {
  largeTitle: ThemeTextStyle;
  title2: ThemeTextStyle;
  title3: ThemeTextStyle;
  headline: ThemeTextStyle;
  body: ThemeTextStyle;
  callout: ThemeTextStyle;
  subhead: ThemeTextStyle;
  footnote: ThemeTextStyle;
  caption: ThemeTextStyle;
}

export interface ThemeFontSize {
  caption: number;
  footnote: number;
  subhead: number;
  callout: number;
  body: number;
  title3: number;
  title2: number;
  largeTitle: number;
}

export interface ThemeFontWeight {
  regular: NonNullable<TextStyle['fontWeight']>;
  semibold: NonNullable<TextStyle['fontWeight']>;
  bold: NonNullable<TextStyle['fontWeight']>;
}

export interface ThemeSpacing {
  xxs: number;
  xs: number;
  sm: number;
  md: number;
  lg: number;
  xl: number;
  xxl: number;
}

export interface ThemeRadius {
  sm: number;
  md: number;
  lg: number;
  xl: number;
  pill: number;
}

export interface ThemeFonts {
  mono: string;
}

export interface Theme {
  colors: ThemeColors;
  typography: ThemeTypography;
  fontSize: ThemeFontSize;
  fontWeight: ThemeFontWeight;
  spacing: ThemeSpacing;
  radius: ThemeRadius;
  fonts: ThemeFonts;
  // Aligns separators and section content with the text column (avatar 40 + gutter 12 + padding 16).
  rowInset: number;
}
