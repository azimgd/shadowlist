import type { Theme } from './Theme';
import { lightTheme } from './lightTheme';
import { semantic } from './macOSPlatformColor';

/*
 * AppKit's own metrics. An AppKit list, menu and form all sit on this type scale:
 * 13pt is `NSFont.systemFontSize`, and `NSLayoutManager.defaultLineHeight` gives the
 * paired line heights. iOS sizing (17pt body) reads oversized next to a toolbar.
 */
const fontSize = {
  caption: 10,
  footnote: 11,
  subhead: 11,
  callout: 12,
  body: 13,
  title3: 15,
  title2: 17,
  largeTitle: 26,
} as const;

const fontWeight = {
  regular: '400',
  semibold: '600',
  bold: '700',
} as const;

const typography = {
  largeTitle: {
    fontSize: fontSize.largeTitle,
    fontWeight: fontWeight.bold,
    lineHeight: 32,
    letterSpacing: 0.22,
  },
  title2: {
    fontSize: fontSize.title2,
    fontWeight: fontWeight.bold,
    lineHeight: 22,
    letterSpacing: 0.3,
  },
  title3: {
    fontSize: fontSize.title3,
    fontWeight: fontWeight.semibold,
    lineHeight: 20,
    letterSpacing: 0.1,
  },
  headline: {
    fontSize: fontSize.body,
    fontWeight: fontWeight.bold,
    lineHeight: 16,
    letterSpacing: 0,
  },
  body: {
    fontSize: fontSize.body,
    fontWeight: fontWeight.regular,
    lineHeight: 16,
    letterSpacing: 0,
  },
  callout: {
    fontSize: fontSize.callout,
    fontWeight: fontWeight.regular,
    lineHeight: 15,
    letterSpacing: 0,
  },
  subhead: {
    fontSize: fontSize.subhead,
    fontWeight: fontWeight.regular,
    lineHeight: 14,
    letterSpacing: 0,
  },
  footnote: {
    fontSize: fontSize.footnote,
    fontWeight: fontWeight.regular,
    lineHeight: 14,
    letterSpacing: 0,
  },
  caption: {
    fontSize: fontSize.caption,
    fontWeight: fontWeight.regular,
    lineHeight: 13,
    letterSpacing: 0,
  },
} as const;

const spacing = {
  xxs: 2,
  xs: 4,
  sm: 6,
  md: 8,
  lg: 12,
  xl: 16,
  xxl: 20,
} as const;

// AppKit content is square-ish: a source-list row carries a 5pt corner, a card 6 to 8.
const radius = {
  sm: 4,
  md: 6,
  lg: 8,
  xl: 10,
  pill: 999,
} as const;

const fonts = {
  // SF Mono under the generic monospace family, which AppKit maps to Menlo anyway.
  mono: 'Menlo',
} as const;

// A macOS table has full-width rows separated by hairlines, not inset cards.
const grouped = {
  inset: 0,
  radius: 0,
  rowInset: 10,
  rowHeight: 28,
} as const;

/*
 * The colors are AppKit semantic colors rather than hex values. `NSColor` resolves one
 * per view at draw time. The same token tracks Dark Mode, an increase-contrast
 * setting, and an inactive or non-key window without a React re-render. Hex tokens would
 * pin the light or dark variant and stay lit when the window loses focus.
 *
 * The two names on a token are a fallback chain, not a blend: AppKit takes the first
 * name it knows. The second entry covers a semantic color that arrived after the
 * oldest macOS this app runs on.
 *
 * Labels carry less than full alpha, and the renderer composites them over the token
 * underneath, which is what AppKit does too. Never stack two of these on each other.
 */
const colors = {
  // The window body. AppKit darkens it while the window is not key.
  background: semantic('windowBackgroundColor'),
  // The surface inside a scroll view: an outline view's or a table's own background.
  elevated: semantic('controlBackgroundColor', 'textBackgroundColor'),
  // The alternating row, which a list uses to group rows without drawing lines.
  elevated2: semantic('underPageBackgroundColor', 'gridColor'),
  // macOS has no card-on-group idiom. The grouped roles collapse to the window.
  groupedBackground: semantic('windowBackgroundColor'),
  groupedCell: semantic('controlBackgroundColor', 'textBackgroundColor'),
  // labelColor dims in an inactive window; headerTextColor stays legible on a bar.
  label: semantic('labelColor'),
  secondaryLabel: semantic('secondaryLabelColor'),
  tertiaryLabel: semantic('tertiaryLabelColor', 'disabledControlTextColor'),
  separator: semantic('separatorColor', 'gridColor'),
  fill: semantic('quaternaryLabelColor', 'separatorColor'),
  // Follows the user's System accent color, the way an AppKit selection does.
  accent: semantic('controlAccentColor', 'selectedContentBackgroundColor'),
  accentSoft: semantic('selectedContentBackgroundColor', 'controlAccentColor'),
  // Readable on any accent color, including a yellow or graphite accent.
  onAccent: semantic('alternateSelectedControlTextColor'),
  blue: semantic('controlAccentColor', 'systemBlueColor'),
  green: semantic('systemGreenColor'),
  orange: semantic('systemOrangeColor'),
  red: semantic('systemRedColor'),
  redSoft: semantic('systemRedColor'),
  avatarPalette: lightTheme.colors.avatarPalette,
} as const;

/*
 * There is no light or dark variant here. Every color resolves itself. One object
 * serves both appearances, and `useTheme` never swaps it.
 */
export const macOSAppKitTheme: Theme = {
  typography,
  fontSize,
  fontWeight,
  spacing,
  radius,
  fonts,
  grouped,
  colors,
  tapTarget: 24,
  rowInset: 10,
};
