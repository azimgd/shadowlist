import UIKit

public final class ShadowListUtilsIndexBadge: UIView {
  public var index: Int { didSet { label.text = String(index); accessibilityLabel = String(index) } }
  public let theme: ShadowListUtilsTheme
  private let label = UILabel()
  public init(index: Int, theme: ShadowListUtilsTheme = .system) {
    self.index = index; self.theme = theme
    super.init(frame: .zero)
    backgroundColor = theme.colors.accentSoft; layer.cornerRadius = 6
    label.text = String(index); label.textAlignment = .center
    label.font = .monospacedDigitSystemFont(ofSize: 12 * ShadowListUtilsPlatform.textScale, weight: .semibold)
    label.textColor = theme.colors.accent; label.isAccessibilityElement = false
    isAccessibilityElement = true; accessibilityLabel = String(index); addSubview(label)
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window)
      label.font = .monospacedDigitSystemFont(ofSize: 12 * ShadowListUtilsPlatform.textScale, weight: .semibold)
    }
  }
  public override var intrinsicContentSize: CGSize { CGSize(width: 22, height: 22) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { intrinsicContentSize }
  public override func layoutSubviews() { super.layoutSubviews(); label.frame = bounds }
}
