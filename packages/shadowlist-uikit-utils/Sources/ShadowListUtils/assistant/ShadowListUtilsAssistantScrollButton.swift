import UIKit

/* Remains mounted while hidden, without intercepting touches or accessibility focus. */
public final class ShadowListUtilsAssistantScrollButton: UIButton {
  public var visible = false { didSet { updateVisibility(animated: window != nil) } }
  public var onPress: (() -> Void)? { didSet { isEnabled = onPress != nil } }
  public var labels = ShadowListUtilsAssistantLabels() { didSet { accessibilityLabel = labels.scrollToLatest } }
  public let theme: ShadowListUtilsTheme

  public init(theme: ShadowListUtilsTheme = .system) {
    self.theme = theme
    super.init(frame: .zero)
    setImage(UIImage(systemName: "chevron.down"), for: .normal)
    tintColor = theme.colors.label
    backgroundColor = theme.colors.background
    layer.cornerRadius = 18
    layer.borderWidth = 1 / UIScreen.main.scale
    layer.borderColor = theme.colors.separator.cgColor
    accessibilityLabel = labels.scrollToLatest
    isEnabled = false
    addAction(UIAction { [weak self] _ in self?.performPress() }, for: .touchUpInside)
    updateVisibility(animated: false)
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize { CGSize(width: 36, height: 36) }
  public override func didMoveToWindow() { super.didMoveToWindow(); updateVisibility(animated: false) }

  func performPress() { guard visible, isEnabled else { return }; onPress?() }

  private func updateVisibility(animated: Bool) {
    isUserInteractionEnabled = visible
    accessibilityElementsHidden = !visible
    isAccessibilityElement = visible
    layer.removeAllAnimations()
    let changes = { self.alpha = self.visible ? 1 : 0; self.transform = self.visible ? .identity : CGAffineTransform(scaleX: 0.85, y: 0.85) }
    if animated && !UIAccessibility.isReduceMotionEnabled { UIView.animate(withDuration: 0.18, animations: changes) } else { changes() }
  }
}
