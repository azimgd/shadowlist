import ShadowListUtils
import UIKit

/*
 * The theme every screen draws with: the library's, following the device appearance.
 */
enum AppTheme {
  static let theme: ShadowListUtilsTheme = {
    var theme = ShadowListUtilsTheme.system
    if #available(iOS 26, *) { theme.grouped.radius = 26 }
    return theme
  }()
  static var colors: ShadowListUtilsThemeColors { theme.colors }
  static var typography: ShadowListUtilsThemeTypography { theme.typography }
}
