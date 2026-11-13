import ShadowListKit
import ShadowListUtils
import UIKit

/*
 * The theme every screen draws with: the library's, following the device appearance.
 */
enum AppTheme {
  static let theme = ShadowListUtilsTheme.system
  static var colors: ShadowListUtilsThemeColors { theme.colors }
  static var typography: ShadowListUtilsThemeTypography { theme.typography }
}

extension ShadowListUtilsTextStyle {
  /*
   * The system font of the style, for the views of the example that are not rows.
   */
  var font: UIFont {
    let weight: UIFont.Weight
    switch fontWeight {
    case .regular: weight = .regular
    case .semibold: weight = .semibold
    case .bold: weight = .bold
    @unknown default: weight = .regular
    }
    return UIFont.systemFont(ofSize: fontSize, weight: weight)
  }

  /*
   * The text for a UILabel in the style: the same font, letter spacing and line height the
   * typeset rows use.
   */
  func string(_ text: String, _ color: UIColor = AppTheme.colors.label) -> NSAttributedString {
    let paragraph = NSMutableParagraphStyle()
    paragraph.minimumLineHeight = lineHeight
    paragraph.maximumLineHeight = lineHeight
    return NSAttributedString(string: text, attributes: [
      .font: font,
      .kern: letterSpacing,
      .paragraphStyle: paragraph,
      .foregroundColor: color,
      // Centers the glyphs in the taller line.
      .baselineOffset: (lineHeight - font.lineHeight) / 4,
    ])
  }
}

/*
 * -SLTextAsync 1 draws the rows' text on a background queue. -SLImages 0 loads no images.
 */
enum Rendering {
  static let asyncText = UserDefaults.standard.string(forKey: "SLTextAsync") == "1"

  static func apply() {
    if asyncText {
      ShadowListKitTextView.appearance().displaysAsynchronously = true
    }
    if !Settings.images {
      ShadowListUtilsImageLoader.shared = NoImageLoader()
    }
  }
}

/*
 * Leaves every image view on its placeholder color.
 */
private final class NoImageLoader: ShadowListUtilsImageLoader {
  override func load(_ url: String, into view: ShadowListUtilsImageView) {}
  override func cancel(_ view: ShadowListUtilsImageView) {}
}
