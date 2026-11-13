import UIKit

/* Three composited opacity pulses; no text/layout work occurs on animation frames. */
public final class ShadowListUtilsAssistantTypingIndicator: UIView {
  public var labels = ShadowListUtilsAssistantLabels() { didSet { accessibilityLabel = labels.typing } }
  public let theme: ShadowListUtilsTheme
  private let dots = (0..<3).map { _ in UIView() }
  private var motionObserver: NSObjectProtocol?

  public init(theme: ShadowListUtilsTheme = .system) {
    self.theme = theme
    super.init(frame: .zero)
    isAccessibilityElement = true
    accessibilityLabel = labels.typing
    accessibilityTraits = .updatesFrequently
    for dot in dots { dot.backgroundColor = theme.colors.secondaryLabel; dot.layer.cornerRadius = 3.5; addSubview(dot) }
    motionObserver = NotificationCenter.default.addObserver(forName: UIAccessibility.reduceMotionStatusDidChangeNotification, object: nil, queue: .main) { [weak self] _ in self?.updatePulse() }
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  deinit { if let motionObserver { NotificationCenter.default.removeObserver(motionObserver) } }
  public override var isHidden: Bool { didSet { updatePulse() } }
  public override var intrinsicContentSize: CGSize { CGSize(width: 21 + 2 * theme.spacing.xs, height: 7 + 2 * theme.spacing.sm) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { intrinsicContentSize }
  public override func layoutSubviews() {
    super.layoutSubviews()
    let direction = ShadowListUtilsDirection(width: bounds.width)
    for (index, dot) in dots.enumerated() {
      dot.frame = direction.place(CGRect(x: CGFloat(index) * (7 + theme.spacing.xs), y: theme.spacing.sm, width: 7, height: 7))
    }
  }
  public override func didMoveToWindow() { super.didMoveToWindow(); updatePulse() }
  private func updatePulse() {
    for (index, dot) in dots.enumerated() {
      dot.layer.removeAnimation(forKey: "pulse")
      guard window != nil, !isHidden, !UIAccessibility.isReduceMotionEnabled else { dot.alpha = 1; continue }
      let pulse = CABasicAnimation(keyPath: "opacity")
      pulse.fromValue = 0.25; pulse.toValue = 1; pulse.duration = 0.6
      pulse.autoreverses = true; pulse.repeatCount = .infinity
      pulse.timingFunction = CAMediaTimingFunction(name: .easeInEaseOut)
      pulse.beginTime = CACurrentMediaTime() + Double(index) * 0.16
      dot.layer.add(pulse, forKey: "pulse")
    }
  }
}
