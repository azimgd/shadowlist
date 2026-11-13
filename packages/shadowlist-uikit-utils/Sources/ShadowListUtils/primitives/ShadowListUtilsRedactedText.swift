import ShadowListKit
import UIKit

/*
 * Text drawn as one bar per line, for a row that stands in for content still loading. The words
 * are typeset in the real style and never shown.
 */
public enum ShadowListUtilsRedactedText {
  /*
   * One bar per line that holds text, in the layout's coordinates: as wide as the line's text,
   * lineHeight * 0.62 tall and centered in its line. A blank line has no bar. A bar's corner
   * radius is half its height.
   */
  public static func bars(for layout: ShadowListKitTextLayout, lineHeight: CGFloat) -> [CGRect] {
    let height = lineHeight * barHeight
    return (0..<Int(layout.lineCount)).compactMap { line in
      let text = layout.rectForLine(at: UInt(line))
      guard text.width > 0 else { return nil }
      return CGRect(x: text.minX, y: CGFloat(line) * lineHeight + (lineHeight - height) / 2, width: text.width, height: height)
    }
  }

  /*
   * Share of a line's height the bar covers, centered to keep the lines apart.
   */
  private static let barHeight: CGFloat = 0.62
}

/*
 * Shows the bars of redacted text. Its frame is the frame the text would have.
 */
final class ShadowListUtilsRedactedTextView: ShadowListUtilsView {
  private var bars: [UIView] = []

  init() {
    super.init(frame: .zero)
    isUserInteractionEnabled = false
    isAccessibilityElement = false
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  func show(_ frames: [CGRect], _ color: ShadowListUtilsColor) {
    while bars.count < frames.count {
      let bar = UIView()
      addSubview(bar)
      bars.append(bar)
    }
    for (index, bar) in bars.enumerated() {
      bar.isHidden = index >= frames.count
      guard index < frames.count else { continue }
      bar.frame = frames[index]
      bar.backgroundColor = color
      bar.layer.cornerRadius = frames[index].height / 2
    }
  }
}
