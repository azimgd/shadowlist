import UIKit

extension ShadowListUtilsThemeColors {
  static let dark = ShadowListUtilsThemeColors(
    background: UIColor(hex: 0x000000),
    elevated: UIColor(hex: 0x1C1C1E),
    elevated2: UIColor(hex: 0x2C2C2E),
    groupedBackground: UIColor(hex: 0x000000),
    groupedCell: UIColor(hex: 0x1C1C1E),
    label: UIColor(hex: 0xFFFFFF),
    secondaryLabel: UIColor(hex: 0xEBEBF5, alpha: 0.6),
    tertiaryLabel: UIColor(hex: 0xEBEBF5, alpha: 0.3),
    separator: UIColor(hex: 0x545458, alpha: 0.6),
    fill: UIColor(hex: 0x767680, alpha: 0.24),
    accent: UIColor(hex: 0x0A84FF),
    accentSoft: UIColor(hex: 0x0A84FF, alpha: 0.16),
    onAccent: UIColor(hex: 0xFFFFFF),
    blue: UIColor(hex: 0x0A84FF),
    green: UIColor(hex: 0x30D158),
    orange: UIColor(hex: 0xFF9F0A),
    red: UIColor(hex: 0xFF453B),
    redSoft: UIColor(hex: 0xFF453B, alpha: 0.16),
    avatarPalette: defaultAvatarPalette)
}

extension ShadowListUtilsTheme {
  public static let dark = ShadowListUtilsTheme(colors: .dark)
}
