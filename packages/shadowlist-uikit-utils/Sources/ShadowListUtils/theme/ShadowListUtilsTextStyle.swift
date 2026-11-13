import ShadowListKit
import UIKit

public enum ShadowListUtilsFontWeight: Hashable {
  case regular
  case semibold
  case bold
}

/*
 * A text style: size, weight, line height and letter spacing of the system font, in points at
 * the system's default text size. Text follows the reader's text size setting. Measuring and
 * drawing use the same typeset lines. A height computed off the main thread is the height the
 * text shows at.
 */
public struct ShadowListUtilsTextStyle: Hashable {
  public var fontSize: CGFloat
  public var fontWeight: ShadowListUtilsFontWeight
  public var lineHeight: CGFloat
  public var letterSpacing: CGFloat

  public init(fontSize: CGFloat, fontWeight: ShadowListUtilsFontWeight, lineHeight: CGFloat, letterSpacing: CGFloat) {
    self.fontSize = fontSize
    self.fontWeight = fontWeight
    self.lineHeight = lineHeight
    self.letterSpacing = letterSpacing
  }

  public func with(_ fontWeight: ShadowListUtilsFontWeight) -> ShadowListUtilsTextStyle {
    ShadowListUtilsTextStyle(fontSize: fontSize, fontWeight: fontWeight, lineHeight: lineHeight, letterSpacing: letterSpacing)
  }

  /*
   * The text typeset at width for a ShadowListKitTextView. maxLines 0 means no limit. Every
   * line is the style's line height tall. Thread safe, row layouts call it off the main thread.
   */
  public func layout(_ text: String, width: CGFloat, maxLines: Int = 0) -> ShadowListKitTextLayout {
    layout(text, width: width, maxLines: maxLines, scaled: true)
  }

  /*
   * The same with the switches the templates use: scaled off keeps the text at its size
   * whatever the text size setting, like initials in an avatar, alignment places every line in
   * width, and weight is a font weight the public weights do not name.
   */
  func layout(
    _ text: String, width: CGFloat, maxLines: Int, scaled: Bool, alignment: NSTextAlignment = .natural, weight: UIFont.Weight? = nil
  ) -> ShadowListKitTextLayout {
    let scale = scaled ? ShadowListUtilsPlatform.textScale : 1
    let font = UIFont.systemFont(ofSize: fontSize * scale, weight: weight ?? self.weight)
    return ShadowListKitTextLayout(
      string: text, font: font, lineHeight: scaledLineHeight(scaled), kern: letterSpacing * scale, width: max(0, width),
      maximumLines: UInt(max(0, maxLines)), alignment: alignment)
  }

  /*
   * Mixed text keeps the same measured line metrics as plain text. Explicit run fonts and
   * colors survive drawing, for titles whose actor and action have different emphasis.
   */
  func layout(_ text: NSAttributedString, width: CGFloat, maxLines: Int = 0) -> ShadowListKitTextLayout {
    let scale = ShadowListUtilsPlatform.textScale
    return ShadowListKitTextLayout(
      attributedString: text, font: UIFont.systemFont(ofSize: fontSize * scale, weight: weight),
      lineHeight: scaledLineHeight(), kern: letterSpacing * scale, width: max(0, width),
      maximumLines: UInt(max(0, maxLines)), alignment: .natural)
  }

  /*
   * The height of one line as text is laid out with it: the style's at the reader's text size,
   * rounded up to the pixel grid.
   */
  func scaledLineHeight(_ scaled: Bool = true) -> CGFloat {
    scaled ? ShadowListUtilsPlatform.ceilToPixel(lineHeight * ShadowListUtilsPlatform.textScale) : lineHeight
  }

  var weight: UIFont.Weight {
    switch fontWeight {
    case .regular: return .regular
    case .semibold: return .semibold
    case .bold: return .bold
    }
  }
}
