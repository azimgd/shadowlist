import UIKit

public final class ShadowListUtilsDashedButton: UIButton {
  public var label: String { didSet { applyStyle() } }
  public var onPress: (() -> Void)? { didSet { updateInteraction() } }
  public var disabled = false { didSet { updateInteraction() } }
  public let theme: ShadowListUtilsTheme
  private let outline = CAShapeLayer()
  private var layoutWidth: CGFloat = -1
  public override var isHighlighted: Bool { didSet { updateFeedback() } }
  public init(label: String, theme: ShadowListUtilsTheme = .system) {
    self.label = label; self.theme = theme
    super.init(frame: .zero)
    backgroundColor = theme.colors.accentSoft; layer.cornerRadius = 10
    outline.fillColor = UIColor.clear.cgColor; outline.strokeColor = theme.colors.accent.cgColor
    outline.lineWidth = 1; outline.lineDashPattern = [4, 3]; layer.addSublayer(outline)
    contentEdgeInsets = UIEdgeInsets(top: 8, left: 0, bottom: 8, right: 0)
    titleLabel?.numberOfLines = 0; titleLabel?.textAlignment = .center; titleLabel?.lineBreakMode = .byWordWrapping
    addAction(UIAction { [weak self] _ in guard let self, !self.disabled else { return }; self.onPress?() }, for: .touchUpInside)
    applyStyle(); updateInteraction()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize {
    let fitted = sizeThatFits(CGSize(width: bounds.width > 0 ? bounds.width : CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude))
    return CGSize(width: UIView.noIntrinsicMetric, height: fitted.height)
  }
  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    let measured = titleLabel?.sizeThatFits(CGSize(width: max(1, size.width), height: CGFloat.greatestFiniteMagnitude)) ?? .zero
    return CGSize(width: size.width, height: max(theme.tapTarget, measured.height + 16))
  }
  public override func layoutSubviews() {
    super.layoutSubviews()
    if layoutWidth != bounds.width { layoutWidth = bounds.width; invalidateIntrinsicContentSize() }
    outline.frame = bounds
    outline.path = UIBezierPath(roundedRect: bounds.insetBy(dx: 0.5, dy: 0.5), cornerRadius: 10).cgPath
  }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window); applyStyle()
    }
  }
  private func applyStyle() {
    setTitle(label, for: .normal); accessibilityLabel = label
    titleLabel?.font = .systemFont(ofSize: theme.typography.body.fontSize * ShadowListUtilsPlatform.textScale, weight: .medium)
    setTitleColor(theme.colors.accent, for: .normal); setTitleColor(theme.colors.accent, for: .disabled)
    let title = theme.typography.body.attributedString(label, color: theme.colors.accent, weight: .medium, alignment: .center, lineBreakMode: .byWordWrapping)
    setAttributedTitle(title, for: .normal); setAttributedTitle(title, for: .disabled)
    invalidateIntrinsicContentSize(); setNeedsLayout()
  }
  private func updateInteraction() { isEnabled = !disabled && onPress != nil; updateFeedback() }
  private func updateFeedback() { alpha = ShadowListUtilsPressFeedback.alpha(pressed: isHighlighted, disabled: disabled) }
}
