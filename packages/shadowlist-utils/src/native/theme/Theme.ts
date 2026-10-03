import type { ColorValue, TextStyle } from 'react-native';

/*
 * A theme color is anything a style prop accepts, not only a CSS string.
 * The macOS token set resolves to AppKit semantic colors, which stay
 * appearance-reactive inside the renderer.
 */
export interface ThemeColors {
  background: ColorValue;
  elevated: ColorValue;
  elevated2: ColorValue;
  groupedBackground: ColorValue;
  groupedCell: ColorValue;
  label: ColorValue;
  secondaryLabel: ColorValue;
  tertiaryLabel: ColorValue;
  separator: ColorValue;
  fill: ColorValue;
  accent: ColorValue;
  accentSoft: ColorValue;
  onAccent: ColorValue;
  blue: ColorValue;
  green: ColorValue;
  orange: ColorValue;
  red: ColorValue;
  redSoft: ColorValue;
  avatarPalette: ReadonlyArray<ColorValue>;
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

/*
 * Metrics of the iOS inset-grouped style: cards of rows on a grouped background.
 */
export interface ThemeGrouped {
  inset: number;
  radius: number;
  rowInset: number;
  rowHeight: number;
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
  grouped: ThemeGrouped;
  tapTarget: number;
  rowInset: number;
}
