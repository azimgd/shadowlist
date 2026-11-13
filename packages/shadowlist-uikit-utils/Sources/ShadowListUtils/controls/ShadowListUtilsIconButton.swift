import UIKit

public final class ShadowListUtilsIconButton: UIControl {
  public var icon: UIView { didSet { oldValue.removeFromSuperview(); installIcon() } }
  public var onPress: (() -> Void)? { didSet { isEnabled = onPress != nil } }
  public let theme: ShadowListUtilsTheme
  public override var isHighlighted: Bool { didSet { alpha = ShadowListUtilsPressFeedback.alpha(pressed: isHighlighted) } }
  public init(icon: UIView, accessibilityLabel: String, theme: ShadowListUtilsTheme = .system) {
    self.icon = icon; self.theme = theme
    super.init(frame: .zero)
    self.accessibilityLabel = accessibilityLabel; accessibilityTraits = .button; isAccessibilityElement = true
    backgroundColor = theme.colors.accentSoft; layer.cornerRadius = 20; isEnabled = false
    addAction(UIAction { [weak self] _ in self?.onPress?() }, for: .touchUpInside)
    installIcon()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize { CGSize(width: 40, height: 40) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { intrinsicContentSize }
  public override func point(inside point: CGPoint, with event: UIEvent?) -> Bool { bounds.insetBy(dx: -4, dy: -4).contains(point) }
  public override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
    let target = super.hitTest(point, with: event)
    return target != nil && !bounds.contains(point) ? self : target
  }
  public override func layoutSubviews() {
    super.layoutSubviews()
    let intrinsic = icon.intrinsicContentSize
    let width = max(0, intrinsic.width >= 0 ? intrinsic.width : icon.bounds.width)
    let height = max(0, intrinsic.height >= 0 ? intrinsic.height : icon.bounds.height)
    icon.frame = CGRect(x: (bounds.width - width) / 2, y: (bounds.height - height) / 2, width: width, height: height)
  }
  private func installIcon() { addSubview(icon); setNeedsLayout() }
}
