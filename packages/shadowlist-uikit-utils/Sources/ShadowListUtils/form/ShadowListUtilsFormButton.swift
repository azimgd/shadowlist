import UIKit

public final class ShadowListUtilsFormButton: UIButton {
  public var label: String { didSet { applyStyle() } }
  public var onPress: (() -> Void)? { didSet { isEnabled = onPress != nil } }
  public var destructive = false { didSet { applyStyle() } }
  public override var isHighlighted: Bool { didSet { alpha = ShadowListUtilsPressFeedback.alpha(pressed: isHighlighted) } }
  public let theme: ShadowListUtilsTheme
  public init(label: String, theme: ShadowListUtilsTheme = .system) {
    self.label = label; self.theme = theme
    super.init(frame: .zero)
    isEnabled = false
    addAction(UIAction { [weak self] _ in self?.onPress?() }, for: .touchUpInside)
    applyStyle()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: max(theme.tapTarget + 6, (titleLabel?.intrinsicContentSize.height ?? 0) + 16)) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    let text = theme.typography.body.layout(label, width: max(1, size.width - 2 * theme.grouped.rowInset), maxLines: ShadowListUtilsPlatform.textScale >= 1.3 ? 2 : 1)
    return CGSize(width: size.width, height: max(theme.tapTarget + 6, text.size.height + 16))
  }
  private func applyStyle() {
    setTitle(label, for: .normal); accessibilityLabel = label
    let color = destructive ? theme.colors.red : theme.colors.accent
    setTitleColor(color, for: .normal)
    let title = theme.typography.body.attributedString(label, color: color, alignment: .center)
    setAttributedTitle(title, for: .normal); setAttributedTitle(title, for: .disabled)
    titleLabel?.font = .systemFont(ofSize: theme.typography.body.fontSize * ShadowListUtilsPlatform.textScale)
    titleLabel?.numberOfLines = ShadowListUtilsPlatform.textScale >= 1.3 ? 2 : 1
    contentEdgeInsets = UIEdgeInsets(top: 8, left: theme.grouped.rowInset, bottom: 8, right: theme.grouped.rowInset)
    titleLabel?.textAlignment = .center
    contentHorizontalAlignment = .center
    backgroundColor = theme.colors.groupedCell
    layer.cornerRadius = theme.grouped.radius
    invalidateIntrinsicContentSize()
  }
}
