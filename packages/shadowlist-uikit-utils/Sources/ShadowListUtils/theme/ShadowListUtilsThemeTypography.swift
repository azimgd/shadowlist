import CoreGraphics

/*
 * The text styles of a theme.
 */
public struct ShadowListUtilsThemeTypography: Hashable {
  public var largeTitle = ShadowListUtilsTextStyle(fontSize: 34, fontWeight: .bold, lineHeight: 41, letterSpacing: 0.37)
  public var title2 = ShadowListUtilsTextStyle(fontSize: 22, fontWeight: .bold, lineHeight: 28, letterSpacing: 0.35)
  public var title3 = ShadowListUtilsTextStyle(fontSize: 20, fontWeight: .semibold, lineHeight: 25, letterSpacing: 0.38)
  public var headline = ShadowListUtilsTextStyle(fontSize: 17, fontWeight: .semibold, lineHeight: 22, letterSpacing: -0.43)
  public var body = ShadowListUtilsTextStyle(fontSize: 17, fontWeight: .regular, lineHeight: 22, letterSpacing: -0.43)
  public var callout = ShadowListUtilsTextStyle(fontSize: 16, fontWeight: .regular, lineHeight: 21, letterSpacing: -0.31)
  public var subhead = ShadowListUtilsTextStyle(fontSize: 15, fontWeight: .regular, lineHeight: 20, letterSpacing: -0.24)
  public var footnote = ShadowListUtilsTextStyle(fontSize: 13, fontWeight: .regular, lineHeight: 18, letterSpacing: -0.08)
  public var caption = ShadowListUtilsTextStyle(fontSize: 12, fontWeight: .regular, lineHeight: 16, letterSpacing: 0)

  public init() {}
}
