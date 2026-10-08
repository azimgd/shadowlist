import ShadowListKit
import UIKit

/*
 * The example's design tokens, the same values as shadowlist-utils' light and dark themes.
 */
enum Theme {
  static func dynamic(_ light: UInt32, _ dark: UInt32, _ lightAlpha: CGFloat = 1, _ darkAlpha: CGFloat = 1) -> UIColor {
    UIColor { traits in
      traits.userInterfaceStyle == .dark ? UIColor(hex: dark, alpha: darkAlpha) : UIColor(hex: light, alpha: lightAlpha)
    }
  }

  static let background = dynamic(0xFFFFFF, 0x000000)
  static let elevated = dynamic(0xF2F2F7, 0x1C1C1E)
  static let elevated2 = dynamic(0xE5E5EA, 0x2C2C2E)
  static let label = dynamic(0x000000, 0xFFFFFF)
  static let secondaryLabel = dynamic(0x3C3C43, 0xEBEBF5, 0.6, 0.6)
  static let tertiaryLabel = dynamic(0x3C3C43, 0xEBEBF5, 0.3, 0.3)
  static let separator = dynamic(0x3C3C43, 0x545458, 0.29, 0.6)
  static let fill = dynamic(0x767680, 0x767680, 0.12, 0.24)
  static let accent = dynamic(0x007AFF, 0x0A84FF)
  static let onAccent = UIColor.white
  static let red = dynamic(0xFF3B30, 0xFF453B)

  static let avatarPalette: [UIColor] = [
    0xFF6B6B, 0x4ECDC4, 0x45B7D1, 0xFFA07A, 0x98D8C8, 0xF7DC6F, 0xBB8FCE, 0x85C1E2, 0xF8B195, 0xC06C84,
  ].map { UIColor(hex: $0, alpha: 1) }

  static let hairline = 1 / UIScreen.main.scale
}

extension UIColor {
  convenience init(hex: UInt32, alpha: CGFloat) {
    self.init(
      red: CGFloat((hex >> 16) & 0xFF) / 255,
      green: CGFloat((hex >> 8) & 0xFF) / 255,
      blue: CGFloat(hex & 0xFF) / 255,
      alpha: alpha)
  }
}

/*
 * A text style: size, weight, line height and letter spacing. Measuring and drawing use the
 * same attributes. A height computed off the main thread matches the label exactly.
 */
struct TextStyle {
  let font: UIFont
  let lineHeight: CGFloat
  let kern: CGFloat

  init(_ size: CGFloat, _ weight: UIFont.Weight, lineHeight: CGFloat, kern: CGFloat) {
    self.font = UIFont.systemFont(ofSize: size, weight: weight)
    self.lineHeight = lineHeight
    self.kern = kern
  }

  func with(_ weight: UIFont.Weight) -> TextStyle {
    TextStyle(font.pointSize, weight, lineHeight: lineHeight, kern: kern)
  }

  static let largeTitle = TextStyle(34, .bold, lineHeight: 41, kern: 0.37)
  static let headline = TextStyle(17, .semibold, lineHeight: 22, kern: -0.43)
  static let body = TextStyle(17, .regular, lineHeight: 22, kern: -0.43)
  static let subhead = TextStyle(15, .regular, lineHeight: 20, kern: -0.24)
  static let footnote = TextStyle(13, .regular, lineHeight: 18, kern: -0.08)
  static let caption = TextStyle(12, .regular, lineHeight: 16, kern: 0)

  func attributes(_ color: UIColor, alignment: NSTextAlignment = .natural, truncate: Bool = false, measuring: Bool = false) -> [NSAttributedString.Key: Any] {
    let paragraph = NSMutableParagraphStyle()
    paragraph.minimumLineHeight = lineHeight
    paragraph.maximumLineHeight = lineHeight
    paragraph.alignment = alignment
    if truncate {
      paragraph.lineBreakMode = .byTruncatingTail
    }
    var attributes: [NSAttributedString.Key: Any] = [
      .font: font,
      .kern: kern,
      .paragraphStyle: paragraph,
      .foregroundColor: color,
    ]
    // Centers the glyphs in the taller line. Measuring leaves it out, it shrinks every line there.
    if !measuring {
      attributes[.baselineOffset] = (lineHeight - font.lineHeight) / 4
    }
    return attributes
  }

  func string(_ text: String, _ color: UIColor = Theme.label, alignment: NSTextAlignment = .natural, truncate: Bool = false) -> NSAttributedString {
    NSAttributedString(string: text, attributes: attributes(color, alignment: alignment, truncate: truncate))
  }

  /*
   * Size of the text wrapped at width. Thread safe, the layouts below run off the main thread.
   */
  func size(_ text: String, width: CGFloat, maxLines: Int = 0) -> CGSize {
    let bounds = NSAttributedString(string: text, attributes: attributes(Theme.label, measuring: true)).boundingRect(
      with: CGSize(width: width, height: .greatestFiniteMagnitude),
      options: [.usesLineFragmentOrigin, .usesFontLeading], context: nil)
    var height = ceil(bounds.height)
    if maxLines > 0 {
      height = min(height, lineHeight * CGFloat(maxLines))
    }
    return CGSize(width: min(width, ceil(bounds.width)), height: height)
  }
}

extension TextStyle {
  /*
   * Typeset text for an SLKTextView. Thread safe, row layouts call it off the main thread.
   */
  func layout(_ text: String, width: CGFloat, maxLines: Int = 0, alignment: NSTextAlignment = .natural) -> SLKTextLayout {
    SLKTextLayout(
      string: text, font: font, lineHeight: lineHeight, kern: kern, width: width,
      maximumLines: UInt(maxLines), alignment: alignment)
  }
}

/*
 * -SLTextAsync 1 draws text on a background queue.
 */
enum TextRendering {
  static let async = UserDefaults.standard.string(forKey: "SLTextAsync") == "1"
}

extension SLKTextView {
  static func make() -> SLKTextView {
    let view = SLKTextView(frame: .zero)
    view.displaysAsynchronously = TextRendering.async
    return view
  }

  func show(_ layout: SLKTextLayout?, _ color: UIColor) {
    textColor = color
    textLayout = layout
  }
}

extension UILabel {
  static func make(lines: Int = 0) -> UILabel {
    let label = UILabel()
    label.numberOfLines = lines
    return label
  }
}
