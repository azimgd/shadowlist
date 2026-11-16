import UIKit

enum ShadowListUtilsChevronIcon {
  enum Direction: Int { case right, down, left, up }

  static func image(size: CGFloat = 17, strokeWidth: CGFloat = 2, direction: Direction = .right) -> UIImage {
    UIGraphicsImageRenderer(size: CGSize(width: size, height: size)).image { context in
      UIColor.black.setFill()
      path(size: size, strokeWidth: strokeWidth, direction: direction).fill()
    }.withRenderingMode(.alwaysTemplate)
  }

  /*
   * The outline in a square of a size: two borders of a turned square, off center toward the tip.
   */
  static func path(size: CGFloat, strokeWidth: CGFloat, direction: Direction) -> UIBezierPath {
    let side = size * 0.42
    let path = path(side: side, strokeWidth: strokeWidth)
    path.apply(
      CGAffineTransform(translationX: size / 2, y: size / 2).rotated(by: CGFloat(direction.rawValue) * .pi / 2)
        .translatedBy(x: -side * 0.3535, y: 0).rotated(by: .pi / 4))
    return path
  }

  static func path(side: CGFloat, strokeWidth: CGFloat) -> UIBezierPath {
    let path = UIBezierPath()
    path.move(to: CGPoint(x: -side / 2, y: -side / 2))
    path.addLine(to: CGPoint(x: side / 2, y: -side / 2))
    path.addLine(to: CGPoint(x: side / 2, y: side / 2))
    path.addLine(to: CGPoint(x: side / 2 - strokeWidth, y: side / 2))
    path.addLine(to: CGPoint(x: side / 2 - strokeWidth, y: -side / 2 + strokeWidth))
    path.addLine(to: CGPoint(x: -side / 2, y: -side / 2 + strokeWidth))
    path.close()
    return path
  }
}
