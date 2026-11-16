import ShadowListUtils
import UIKit

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
    attributedString(text, color: color)
  }
}
