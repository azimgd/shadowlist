import ShadowListUtilsCore
import UIKit

/* Decorative only: progress accessibility belongs to the labelled bar, not this ring. */
public final class ShadowListUtilsProgressRing: UIView {
  public var fraction: Double { didSet { setNeedsDisplay() } }
  public var size: CGFloat { didSet { invalidateIntrinsicContentSize(); setNeedsDisplay() } }
  public var strokeWidth: CGFloat { didSet { setNeedsDisplay() } }
  public var color: UIColor? { didSet { setNeedsDisplay() } }
  public var trackColor: UIColor? { didSet { setNeedsDisplay() } }
  public let theme: ShadowListUtilsTheme

  public init(fraction: Double, size: CGFloat = 52, strokeWidth: CGFloat = 7, theme: ShadowListUtilsTheme = .system) {
    self.fraction = fraction; self.size = size; self.strokeWidth = strokeWidth; self.theme = theme
    super.init(frame: .zero)
    isOpaque = false; backgroundColor = .clear
    isAccessibilityElement = false; accessibilityElementsHidden = true; isUserInteractionEnabled = false
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize { CGSize(width: max(0, size), height: max(0, size)) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { intrinsicContentSize }
  public override func draw(_ rect: CGRect) {
    let diameter = min(bounds.width, bounds.height)
    let stroke = min(diameter, max(0, strokeWidth))
    guard diameter > 0, stroke > 0 else { return }
    let center = CGPoint(x: bounds.midX, y: bounds.midY)
    let radius = (diameter - stroke) / 2
    let track = UIBezierPath(arcCenter: center, radius: radius, startAngle: -.pi / 2, endAngle: .pi * 1.5, clockwise: true)
    track.lineWidth = stroke; (trackColor ?? theme.colors.accentSoft).setStroke(); track.stroke()
    let value = ShadowListUtilsCore.clampFraction(fraction)
    guard value > 0 else { return }
    let arc = UIBezierPath(arcCenter: center, radius: radius, startAngle: -.pi / 2,
      endAngle: -.pi / 2 + CGFloat(value) * 2 * .pi, clockwise: true)
    arc.lineWidth = stroke; arc.lineCapStyle = .round
    (color ?? theme.colors.accent).setStroke(); arc.stroke()
  }
}
