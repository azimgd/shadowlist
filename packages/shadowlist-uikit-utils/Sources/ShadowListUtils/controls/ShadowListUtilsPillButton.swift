import UIKit

public final class ShadowListUtilsPillButton: UIButton {
  public var label: String { didSet { applyStyle() } }
  public var onPress: (() -> Void)? { didSet { updateInteraction() } }
  public var variant: ShadowListUtilsPillButtonVariant = .primary { didSet { applyStyle() } }
  public var disabled = false { didSet { updateInteraction() } }
  public let theme: ShadowListUtilsTheme
  public override var isHighlighted: Bool { didSet { updateFeedback() } }

  public init(label: String, theme: ShadowListUtilsTheme = .system) {
    self.label = label; self.theme = theme
    super.init(frame: .zero)
    layer.cornerRadius = 20
    contentEdgeInsets = UIEdgeInsets(top: 0, left: 14, bottom: 0, right: 14)
    addAction(UIAction { [weak self] _ in guard let self, !self.disabled else { return }; self.onPress?() }, for: .touchUpInside)
    applyStyle(); updateInteraction()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  // The pill is as wide as its title on whole pixels and its padding. A button alone rounds its title up to a point.
  public override var intrinsicContentSize: CGSize {
    let title = ShadowListUtilsPlatform.ceilToPixel(currentAttributedTitle?.size().width ?? 0)
    return CGSize(width: max(72, contentEdgeInsets.left + title + contentEdgeInsets.right), height: 40)
  }
  public override func titleRect(forContentRect contentRect: CGRect) -> CGRect {
    ShadowListUtilsPlatform.centeredTitleRect(super.titleRect(forContentRect: contentRect), title: currentAttributedTitle, in: contentRect)
  }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { intrinsicContentSize }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window); applyStyle()
    }
  }
  private func applyStyle() {
    setTitle(label, for: .normal); accessibilityLabel = label
    let emphasized = variant == .primary || variant == .tinted
    titleLabel?.font = .systemFont(ofSize: theme.typography.body.fontSize * ShadowListUtilsPlatform.textScale, weight: emphasized ? .semibold : .regular)
    switch variant {
    case .primary: backgroundColor = theme.colors.accent; setTitleColor(theme.colors.onAccent, for: .normal)
    case .tinted: backgroundColor = theme.colors.fill; setTitleColor(theme.colors.accent, for: .normal)
    case .plain: backgroundColor = theme.colors.background; setTitleColor(theme.colors.secondaryLabel, for: .normal)
    case .destructive: backgroundColor = theme.colors.background; setTitleColor(theme.colors.red, for: .normal)
    }
    let title = theme.typography.body.attributedString(label, color: titleColor(for: .normal) ?? theme.colors.label, weight: emphasized ? .semibold : nil, alignment: .center)
    setAttributedTitle(title, for: .normal); setAttributedTitle(title, for: .disabled)
    setTitleColor(titleColor(for: .normal), for: .disabled)
    invalidateIntrinsicContentSize()
  }
  private func updateInteraction() { isEnabled = !disabled && onPress != nil; updateFeedback() }
  private func updateFeedback() { alpha = ShadowListUtilsPressFeedback.alpha(pressed: isHighlighted, disabled: disabled) }
}
