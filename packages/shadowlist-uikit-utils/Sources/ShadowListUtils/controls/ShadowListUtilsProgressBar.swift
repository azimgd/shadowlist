import ShadowListUtilsCore
import UIKit

public final class ShadowListUtilsProgressBar: UIView {
  public var fraction: Double { didSet { refresh() } }
  public var color: UIColor? { didSet { setNeedsDisplay() } }
  public var height: CGFloat = 6 { didSet { invalidateIntrinsicContentSize(); setNeedsDisplay() } }
  public let theme: ShadowListUtilsTheme

  public init(fraction: Double, accessibilityLabel: String, theme: ShadowListUtilsTheme = .system) {
    self.fraction = fraction; self.theme = theme
    super.init(frame: .zero)
    self.accessibilityLabel = accessibilityLabel
    isOpaque = false; backgroundColor = .clear; isAccessibilityElement = true
    accessibilityTraits = .updatesFrequently
    refresh()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: max(0, height)) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { CGSize(width: size.width, height: max(0, height)) }
  public override func draw(_ rect: CGRect) {
    guard bounds.width > 0, bounds.height > 0 else { return }
    let track = UIBezierPath(roundedRect: bounds, cornerRadius: bounds.height / 2)
    theme.colors.fill.setFill(); track.fill(); track.addClip()
    var fill = CGRect(x: bounds.minX, y: bounds.minY, width: bounds.width * CGFloat(ShadowListUtilsCore.clampFraction(fraction)), height: bounds.height)
    if effectiveUserInterfaceLayoutDirection == .rightToLeft { fill = fill.mirrored(in: bounds.width) }
    (color ?? theme.colors.accent).setFill()
    UIBezierPath(roundedRect: fill, cornerRadius: bounds.height / 2).fill()
  }
  private func refresh() {
    accessibilityValue = "\(ShadowListUtilsCore.progressPercent(fraction))%"
    setNeedsDisplay()
  }
}
