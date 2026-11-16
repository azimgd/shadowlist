import UIKit

/*
 * The three icons the rows draw: the grip of a reorderable row, the chevron of a row that opens
 * something and the check of a selected row. Each fills its view, which is square.
 */
final class ShadowListUtilsIconView: ShadowListUtilsView {
  enum Kind {
    case grip
    case chevron
    case check
  }

  let kind: Kind

  var color: ShadowListUtilsColor = .clear {
    didSet { setNeedsDisplay() }
  }

  init(kind: Kind) {
    self.kind = kind
    super.init(frame: .zero)
    isOpaque = false
    backgroundColor = .clear
    contentMode = .redraw
    isUserInteractionEnabled = false
    isAccessibilityElement = false
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func draw(_ rect: CGRect) {
    let path = Self.path(kind, size: bounds.width, isRightToLeft: ShadowListUtilsPlatform.isRightToLeft)
    if kind == .check {
      color.setStroke()
      path.stroke()
    } else {
      color.setFill()
      path.fill()
    }
  }

  /*
   * The icon's shape in a square of a size. The check is stroked and the others are filled. A
   * chevron points left in a layout that runs from right to left.
   */
  static func path(_ kind: Kind, size: CGFloat, isRightToLeft: Bool = false) -> UIBezierPath {
    let path = UIBezierPath()
    switch kind {
    case .grip:
      // Three lines as wide as the icon, gripGapShare of its size apart.
      let step = gripStroke + size * gripGapShare
      let top = (size - (3 * gripStroke + 2 * size * gripGapShare)) / 2
      for line in 0..<3 {
        let frame = CGRect(x: 0, y: top + CGFloat(line) * step, width: size, height: gripStroke)
        path.append(UIBezierPath(roundedRect: frame, cornerRadius: gripStroke / 2))
      }
    case .chevron:
      // The outline of the ChevronIcon, with its tip along the layout.
      return ShadowListUtilsChevronIcon.path(size: size, strokeWidth: chevronStroke, direction: isRightToLeft ? .left : .right)
    case .check:
      // A short arm and a long one meeting at the bottom, the long one rising to the right.
      let short = size * checkShortShare * halfDiagonal
      let long = size * checkLongShare * halfDiagonal
      let corner = CGPoint(x: size / 2 + (short - long) / 2, y: size / 2 + (short + long) / 2 - size * checkLiftShare)
      path.move(to: CGPoint(x: corner.x - short, y: corner.y - short))
      path.addLine(to: corner)
      path.addLine(to: CGPoint(x: corner.x + long, y: corner.y - long))
      path.lineWidth = checkStroke
    }
    return path
  }

  private static let halfDiagonal: CGFloat = 0.70710677
  private static let gripStroke: CGFloat = 1.75
  private static let gripGapShare: CGFloat = 0.22
  private static let chevronStroke: CGFloat = 2
  private static let checkStroke: CGFloat = 2.2
  private static let checkShortShare: CGFloat = 0.26
  private static let checkLongShare: CGFloat = 0.5
  private static let checkLiftShare: CGFloat = 0.06
}
