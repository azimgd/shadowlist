import ShadowListKit
import UIKit

/*
 * TextKit draws the same line boxes as React Native. The kit retains line heights and hit testing.
 */
final class ShadowListUtilsTextLayout: ShadowListKitTextLayout, @unchecked Sendable {
  private let layout: ShadowListKitTextLayout
  private let text: NSAttributedString
  private let width: CGFloat
  private let maxLines: Int
  private var measuredSize: CGSize = .zero

  init(layout: ShadowListKitTextLayout, text: NSAttributedString, width: CGFloat, maxLines: Int) {
    self.layout = layout
    self.text = text
    self.width = width
    self.maxLines = maxLines
    super.init()
    let storage = makeTextStorage()
    let manager = storage.layoutManagers[0]
    let container = manager.textContainers[0]
    manager.ensureLayout(for: container)
    let glyphs = manager.glyphRange(for: container)
    let truncated = glyphs.length > 0 && manager.truncatedGlyphRange(inLineFragmentForGlyphAt: NSMaxRange(glyphs) - 1).location != NSNotFound
    let measuredWidth = truncated ? width : min(width, ShadowListUtilsPlatform.ceilToPixel(manager.usedRect(for: container).width))
    measuredSize = CGSize(width: measuredWidth, height: layout.size.height)
  }

  override var size: CGSize { measuredSize }
  override var lineCount: UInt { layout.lineCount }
  override func rectForLine(at index: UInt) -> CGRect { layout.rectForLine(at: index) }
  override func characterIndex(at point: CGPoint) -> UInt { layout.characterIndex(at: point) }
  override func draw(at point: CGPoint, in context: CGContext, color: CGColor) {
    let storage = makeTextStorage()
    text.enumerateAttribute(.foregroundColor, in: NSRange(location: 0, length: text.length)) { value, range, _ in
      if value == nil { storage.addAttribute(.foregroundColor, value: UIColor(cgColor: color), range: range) }
    }
    let manager = storage.layoutManagers[0]
    UIGraphicsPushContext(context)
    manager.drawGlyphs(forGlyphRange: manager.glyphRange(for: manager.textContainers[0]), at: point)
    UIGraphicsPopContext()
  }

  private func makeTextStorage() -> NSTextStorage {
    let storage = NSTextStorage(attributedString: text)
    let manager = NSLayoutManager()
    manager.usesFontLeading = false
    let container = NSTextContainer(size: CGSize(width: width, height: .greatestFiniteMagnitude))
    container.lineFragmentPadding = 0
    container.maximumNumberOfLines = maxLines
    container.lineBreakMode = maxLines > 0 ? .byTruncatingTail : .byClipping
    manager.addTextContainer(container)
    storage.addLayoutManager(manager)
    return storage
  }
}
