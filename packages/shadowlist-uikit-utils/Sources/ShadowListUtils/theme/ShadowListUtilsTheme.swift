import UIKit

/*
 * The gaps between and around things.
 */
public struct ShadowListUtilsThemeSpacing: Hashable {
  public var xxs: CGFloat = 2
  public var xs: CGFloat = 4
  public var sm: CGFloat = 8
  public var md: CGFloat = 12
  public var lg: CGFloat = 16
  public var xl: CGFloat = 20
  public var xxl: CGFloat = 24

  public init() {}
}

/*
 * The corner radii. pill rounds a corner all the way.
 */
public struct ShadowListUtilsThemeRadius: Hashable {
  public var sm: CGFloat = 8
  public var md: CGFloat = 12
  public var lg: CGFloat = 16
  public var xl: CGFloat = 20
  public var pill: CGFloat = 999

  public init() {}
}

public enum ShadowListUtilsCaptionCase: Hashable {
  case upper
  case none
}

/*
 * The numbers of grouped rows and their captions.
 */
public struct ShadowListUtilsThemeGrouped: Hashable {
  public var inset: CGFloat = 16
  public var radius: CGFloat = 10
  public var rowInset: CGFloat = 16
  public var rowHeight: CGFloat = 44
  public var captionCase: ShadowListUtilsCaptionCase = .upper
  public var chevron = true

  public init() {}
}

/*
 * The design tokens the templates draw with: the same values as the light and dark themes of
 * shadowlist-utils. A theme is a value. Copy one and change its fields to override it:
 *
 *   var theme = ShadowListUtilsTheme.light
 *   theme.colors.accent = .orange
 *
 * A row holds the theme it was built with. There is no global current theme.
 * rowInset is where a row's text starts after its avatar, and where its separator starts.
 * tapTarget is the smallest side of something a finger presses.
 */
public struct ShadowListUtilsTheme: Hashable {
  public var colors: ShadowListUtilsThemeColors
  public var typography: ShadowListUtilsThemeTypography
  public var spacing: ShadowListUtilsThemeSpacing
  public var radius: ShadowListUtilsThemeRadius
  public var grouped: ShadowListUtilsThemeGrouped
  public var rowInset: CGFloat
  public var tapTarget: CGFloat
  public var pressFeedback: ShadowListUtilsThemePressFeedback = .highlight

  public init(
    colors: ShadowListUtilsThemeColors,
    typography: ShadowListUtilsThemeTypography = ShadowListUtilsThemeTypography(),
    spacing: ShadowListUtilsThemeSpacing = ShadowListUtilsThemeSpacing(),
    radius: ShadowListUtilsThemeRadius = ShadowListUtilsThemeRadius(),
    grouped: ShadowListUtilsThemeGrouped = ShadowListUtilsThemeGrouped(),
    rowInset: CGFloat = 68,
    tapTarget: CGFloat = 44
  ) {
    self.colors = colors
    self.typography = typography
    self.spacing = spacing
    self.radius = radius
    self.grouped = grouped
    self.rowInset = rowInset
    self.tapTarget = tapTarget
  }

  /*
   * Follows the device appearance: light colors in light mode and dark colors in dark mode.
   */
  public static let system = ShadowListUtilsTheme(colors: .dynamic(light: .light, dark: .dark))

  /*
   * The thickness of a separator: one device pixel.
   */
  public static var hairline: CGFloat { ShadowListUtilsPlatform.hairline }
}
