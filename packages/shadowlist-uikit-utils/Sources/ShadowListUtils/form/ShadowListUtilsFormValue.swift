import UIKit

/* A trailing-aligned read-only value; the empty value paints an em dash. */
public final class ShadowListUtilsFormValue: UIView {
  public var text: String { didSet { label.text = text.isEmpty ? "—" : text; invalidateIntrinsicContentSize() } }
  public let theme: ShadowListUtilsTheme
  private let label = UILabel()
  public init(text: String, theme: ShadowListUtilsTheme = .system) {
    self.text = text; self.theme = theme
    super.init(frame: .zero)
    label.font = .monospacedDigitSystemFont(ofSize: theme.typography.body.fontSize * ShadowListUtilsPlatform.textScale, weight: .regular)
    label.text = text.isEmpty ? "—" : text
    label.textColor = theme.colors.secondaryLabel
    label.textAlignment = .right; label.numberOfLines = 1
    label.lineBreakMode = .byTruncatingTail
    addSubview(label)
    isAccessibilityElement = true
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var accessibilityLabel: String? {
    get { super.accessibilityLabel ?? (text.isEmpty ? "—" : text) }
    set { super.accessibilityLabel = newValue }
  }
  public override var intrinsicContentSize: CGSize { label.intrinsicContentSize }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { label.sizeThatFits(size) }
  public override func layoutSubviews() { super.layoutSubviews(); label.frame = bounds }
}
