import UIKit

/*
 * The colors of a theme. Alpha values are part of the color.
 */
public struct ShadowListUtilsThemeColors: Hashable {
  public var background: UIColor
  public var elevated: UIColor
  public var elevated2: UIColor
  public var groupedBackground: UIColor
  public var groupedCell: UIColor
  public var label: UIColor
  public var secondaryLabel: UIColor
  public var tertiaryLabel: UIColor
  public var separator: UIColor
  public var fill: UIColor
  public var accent: UIColor
  public var accentSoft: UIColor
  public var onAccent: UIColor
  public var blue: UIColor
  public var green: UIColor
  public var orange: UIColor
  public var red: UIColor
  public var redSoft: UIColor
  public var avatarPalette: [UIColor]

  public init(
    background: UIColor,
    elevated: UIColor,
    elevated2: UIColor,
    groupedBackground: UIColor,
    groupedCell: UIColor,
    label: UIColor,
    secondaryLabel: UIColor,
    tertiaryLabel: UIColor,
    separator: UIColor,
    fill: UIColor,
    accent: UIColor,
    accentSoft: UIColor,
    onAccent: UIColor,
    blue: UIColor,
    green: UIColor,
    orange: UIColor,
    red: UIColor,
    redSoft: UIColor,
    avatarPalette: [UIColor]
  ) {
    self.background = background
    self.elevated = elevated
    self.elevated2 = elevated2
    self.groupedBackground = groupedBackground
    self.groupedCell = groupedCell
    self.label = label
    self.secondaryLabel = secondaryLabel
    self.tertiaryLabel = tertiaryLabel
    self.separator = separator
    self.fill = fill
    self.accent = accent
    self.accentSoft = accentSoft
    self.onAccent = onAccent
    self.blue = blue
    self.green = green
    self.orange = orange
    self.red = red
    self.redSoft = redSoft
    self.avatarPalette = avatarPalette
  }

  /*
   * Colors that follow the device appearance: each one resolves to its light or dark value for
   * the trait collection it is drawn in.
   */
  static func dynamic(light: ShadowListUtilsThemeColors, dark: ShadowListUtilsThemeColors) -> ShadowListUtilsThemeColors {
    func pair(_ path: KeyPath<ShadowListUtilsThemeColors, UIColor>) -> UIColor {
      let (lightColor, darkColor) = (light[keyPath: path], dark[keyPath: path])
      return UIColor { $0.userInterfaceStyle == .dark ? darkColor : lightColor }
    }
    return ShadowListUtilsThemeColors(
      background: pair(\.background),
      elevated: pair(\.elevated),
      elevated2: pair(\.elevated2),
      groupedBackground: pair(\.groupedBackground),
      groupedCell: pair(\.groupedCell),
      label: pair(\.label),
      secondaryLabel: pair(\.secondaryLabel),
      tertiaryLabel: pair(\.tertiaryLabel),
      separator: pair(\.separator),
      fill: pair(\.fill),
      accent: pair(\.accent),
      accentSoft: pair(\.accentSoft),
      onAccent: pair(\.onAccent),
      blue: pair(\.blue),
      green: pair(\.green),
      orange: pair(\.orange),
      red: pair(\.red),
      redSoft: pair(\.redSoft),
      avatarPalette: zip(light.avatarPalette, dark.avatarPalette).map { lightColor, darkColor in
        UIColor { $0.userInterfaceStyle == .dark ? darkColor : lightColor }
      })
  }
}

extension UIColor {
  /*
   * A color from 0xRRGGBB and an alpha.
   */
  convenience init(hex: UInt32, alpha: CGFloat = 1) {
    self.init(
      red: CGFloat((hex >> 16) & 0xFF) / 255,
      green: CGFloat((hex >> 8) & 0xFF) / 255,
      blue: CGFloat(hex & 0xFF) / 255,
      alpha: alpha)
  }
}
