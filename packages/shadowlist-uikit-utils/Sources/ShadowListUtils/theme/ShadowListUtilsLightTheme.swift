import UIKit

extension ShadowListUtilsThemeColors {
  /*
   * The palette avatars and tiles pick from by name, the same in both themes.
   */
  static let defaultAvatarPalette: [UIColor] = [
    0xFF6B6B, 0x4ECDC4, 0x45B7D1, 0xFFA07A, 0x98D8C8, 0xF7DC6F, 0xBB8FCE, 0x85C1E2, 0xF8B195, 0xC06C84,
  ].map { UIColor(hex: $0) }

  static let light = ShadowListUtilsThemeColors(
    background: UIColor(hex: 0xFFFFFF),
    elevated: UIColor(hex: 0xF2F2F7),
    elevated2: UIColor(hex: 0xE5E5EA),
    groupedBackground: UIColor(hex: 0xF2F2F7),
    groupedCell: UIColor(hex: 0xFFFFFF),
    label: UIColor(hex: 0x000000),
    secondaryLabel: UIColor(hex: 0x3C3C43, alpha: 0.6),
    tertiaryLabel: UIColor(hex: 0x3C3C43, alpha: 0.3),
    separator: UIColor(hex: 0x3C3C43, alpha: 0.29),
    fill: UIColor(hex: 0x767680, alpha: 0.12),
    accent: UIColor(hex: 0x007AFF),
    accentSoft: UIColor(hex: 0x007AFF, alpha: 0.16),
    onAccent: UIColor(hex: 0xFFFFFF),
    blue: UIColor(hex: 0x007AFF),
    green: UIColor(hex: 0x34C759),
    orange: UIColor(hex: 0xFF9500),
    red: UIColor(hex: 0xFF3B30),
    redSoft: UIColor(hex: 0xFF3B30, alpha: 0.16),
    avatarPalette: defaultAvatarPalette)
}

extension ShadowListUtilsTheme {
  public static let light = ShadowListUtilsTheme(colors: .light)
}
