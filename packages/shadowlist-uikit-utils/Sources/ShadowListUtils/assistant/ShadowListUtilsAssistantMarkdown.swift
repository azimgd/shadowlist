import UIKit

#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * Markdown from the existing shared parser. Stable blocks keep their views as the last block grows.
 */
public final class ShadowListUtilsAssistantMarkdown: UIView {
  public var text: String { didSet { if text != oldValue { rebuild() } } }
  public var streaming = false { didSet { updateMotion(); setNeedsLayout(); invalidateIntrinsicContentSize() } }
  public var onCopyCode: ((String) -> Void)? { didSet { updateCallbacks() } }
  public var onOpenLink: ((String) -> Void)? { didSet { updateCallbacks() } }
  public var labels = ShadowListUtilsAssistantLabels() { didSet { rebuild(keepingViews: false) } }
  public let theme: ShadowListUtilsTheme
  private var blocks: [ShadowListUtilsAssistantMarkdownBlock] = []
  private let dot = UIView()

  public init(text: String = "", theme: ShadowListUtilsTheme = .system) {
    self.text = text
    self.theme = theme
    super.init(frame: .zero)
    dot.backgroundColor = theme.colors.secondaryLabel
    dot.layer.cornerRadius = 3
    dot.isAccessibilityElement = false
    addSubview(dot)
    rebuild()
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: size.width, height: place(width: size.width))
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    _ = place(width: bounds.width)
  }

  public override func didMoveToWindow() {
    super.didMoveToWindow()
    updateMotion()
  }

  private func updateMotion() {
    dot.layer.removeAnimation(forKey: "pulse")
    guard streaming, window != nil, !UIAccessibility.isReduceMotionEnabled else { return }
    let pulse = CABasicAnimation(keyPath: "opacity")
    pulse.fromValue = 0.35
    pulse.toValue = 1
    pulse.duration = 0.8
    pulse.autoreverses = true
    pulse.repeatCount = .infinity
    dot.layer.add(pulse, forKey: "pulse")
  }

  private func rebuild(keepingViews: Bool = true) {
    let previous = Dictionary(uniqueKeysWithValues: blocks.map { ($0.block.key, $0) })
    blocks = ShadowListUtilsCore.parseMarkdown(text).map { block in
      if keepingViews, let view = previous[block.key], view.block.raw == block.raw { return view }
      return ShadowListUtilsAssistantMarkdownBlock(block: block, theme: theme, labels: labels)
    }
    for view in previous.values where !blocks.contains(where: { $0 === view }) { view.removeFromSuperview() }
    blocks.forEach { if $0.superview == nil { addSubview($0) } }
    updateCallbacks()
    setNeedsLayout()
    invalidateIntrinsicContentSize()
  }

  private func updateCallbacks() {
    for block in blocks { block.onCopyCode = onCopyCode; block.onOpenLink = onOpenLink }
  }

  private func place(width: CGFloat) -> CGFloat {
    var y: CGFloat = 0
    for block in blocks {
      let height = block.sizeThatFits(CGSize(width: width, height: .greatestFiniteMagnitude)).height
      block.frame = CGRect(x: 0, y: y, width: width, height: height)
      y += height + theme.spacing.md
    }
    dot.isHidden = !streaming
    if streaming { dot.frame = CGRect(x: 0, y: y, width: 6, height: 6); y += 6 + theme.spacing.md }
    return max(0, y - theme.spacing.md)
  }
}
