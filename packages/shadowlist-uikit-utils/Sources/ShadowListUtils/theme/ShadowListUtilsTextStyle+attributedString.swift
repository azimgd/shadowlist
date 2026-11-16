import UIKit

/*
 * Native widget labels use the same resolved style metrics as the row typesetter.
 */
extension ShadowListUtilsTextStyle {
  public func attributedString(_ text: String, color: UIColor, weight: UIFont.Weight? = nil, alignment: NSTextAlignment = .natural, lineBreakMode: NSLineBreakMode = .byTruncatingTail) -> NSAttributedString {
    NSAttributedString(string: text, attributes: attributes(color: color, weight: weight, alignment: alignment, lineBreakMode: lineBreakMode))
  }

  /*
   * The attributes of a line in the style. A text input passes halfLeading false: React Native's
   * TextInput draws its lines without the half-leading its Text applies.
   */
  func attributes(
    color: UIColor, weight: UIFont.Weight? = nil, alignment: NSTextAlignment = .natural, lineBreakMode: NSLineBreakMode = .byTruncatingTail,
    halfLeading: Bool = true
  ) -> [NSAttributedString.Key: Any] {
    let scale = ShadowListUtilsPlatform.textScale
    let font = UIFont.systemFont(ofSize: fontSize * scale, weight: weight ?? self.weight)
    let paragraph = NSMutableParagraphStyle()
    paragraph.minimumLineHeight = scaledLineHeight()
    paragraph.maximumLineHeight = paragraph.minimumLineHeight
    paragraph.alignment = alignment; paragraph.lineBreakMode = lineBreakMode
    return [
      .font: font,
      .baselineOffset: halfLeading ? Self.labelBaselineOffset(font, lineHeight: scaledLineHeight()) : 0,
      .kern: letterSpacing * scale, .paragraphStyle: paragraph, .foregroundColor: color,
    ]
  }
}
