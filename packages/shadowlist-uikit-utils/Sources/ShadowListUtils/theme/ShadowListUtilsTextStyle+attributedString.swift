import UIKit

/* Native widget labels use the same resolved style metrics as the row typesetter. */
extension ShadowListUtilsTextStyle {
  func attributedString(_ text: String, color: UIColor, weight: UIFont.Weight? = nil, alignment: NSTextAlignment = .natural, lineBreakMode: NSLineBreakMode = .byTruncatingTail) -> NSAttributedString {
    let scale = ShadowListUtilsPlatform.textScale
    let paragraph = NSMutableParagraphStyle()
    paragraph.minimumLineHeight = scaledLineHeight()
    paragraph.maximumLineHeight = paragraph.minimumLineHeight
    paragraph.alignment = alignment; paragraph.lineBreakMode = lineBreakMode
    return NSAttributedString(string: text, attributes: [
      .font: UIFont.systemFont(ofSize: fontSize * scale, weight: weight ?? self.weight),
      .kern: letterSpacing * scale, .paragraphStyle: paragraph, .foregroundColor: color,
    ])
  }
}
