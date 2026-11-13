import UIKit

/*
 * Selectable disclosure text has one TextKit configuration for background measurement and
 * native selection/drawing. Layout instances contain immutable text, never a UIView or a
 * shared mutable text storage/layout manager.
 */
final class ShadowListUtilsAssistantDisclosureTextLayout {
  let text: NSAttributedString
  let size: CGSize
  let maximumLines: Int
  let selectable: Bool

  init(text: String, width: CGFloat, style: ShadowListUtilsTextStyle, theme: ShadowListUtilsTheme, expanded: Bool, monospaced: Bool = false) {
    maximumLines = expanded ? 0 : 1
    selectable = expanded
    let scale = ShadowListUtilsPlatform.textScale
    let paragraph = NSMutableParagraphStyle()
    paragraph.minimumLineHeight = style.lineHeight * scale
    paragraph.maximumLineHeight = paragraph.minimumLineHeight
    paragraph.lineBreakMode = expanded ? .byWordWrapping : .byTruncatingHead
    let font = monospaced ? UIFont.monospacedSystemFont(ofSize: style.fontSize * scale, weight: .regular) : UIFont.systemFont(ofSize: style.fontSize * scale)
    self.text = NSAttributedString(string: text, attributes: [
      .font: font, .kern: style.letterSpacing * scale, .paragraphStyle: paragraph,
      .foregroundColor: theme.colors.secondaryLabel,
    ])
    let storage = NSTextStorage(attributedString: self.text)
    let manager = NSLayoutManager()
    manager.usesFontLeading = false
    let container = NSTextContainer(size: CGSize(width: max(1, width), height: CGFloat.greatestFiniteMagnitude))
    container.lineFragmentPadding = 0
    container.maximumNumberOfLines = maximumLines
    container.lineBreakMode = paragraph.lineBreakMode
    storage.addLayoutManager(manager)
    manager.addTextContainer(container)
    manager.ensureLayout(for: container)
    let used = manager.usedRect(for: container)
    let height = max(used.maxY, manager.extraLineFragmentRect.maxY)
    size = CGSize(width: max(1, width), height: ShadowListUtilsPlatform.ceilToPixel(height))
  }

  func apply(to view: UITextView) {
    view.isEditable = false
    view.isSelectable = selectable
    view.isScrollEnabled = false
    view.backgroundColor = .clear
    view.textContainerInset = .zero
    view.textContainer.lineFragmentPadding = 0
    view.layoutManager.usesFontLeading = false
    view.textContainer.maximumNumberOfLines = maximumLines
    view.textContainer.lineBreakMode = maximumLines == 1 ? .byTruncatingHead : .byWordWrapping
    view.textContainer.size = CGSize(width: size.width, height: CGFloat.greatestFiniteMagnitude)
    view.dataDetectorTypes = []
    if text.length > 0 { view.typingAttributes = text.attributes(at: text.length - 1, effectiveRange: nil) }
    if view.attributedText?.isEqual(to: text) != true {
      let sameText = view.attributedText?.string == text.string
      let selection = view.selectedRange
      view.attributedText = text
      if sameText && selectable && selection.location != NSNotFound && NSMaxRange(selection) <= text.length {
        view.selectedRange = selection
      }
    }
  }
}
