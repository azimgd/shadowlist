import UIKit

public enum ShadowListUtilsRetryIcon {
  public static func image(size: CGFloat = 20, strokeWidth: CGFloat = 1.8) -> UIImage {
    UIGraphicsImageRenderer(size: CGSize(width: size, height: size)).image { context in
      UIColor.black.setStroke()
      UIColor.black.setFill()
      let ring = size * 0.6
      let arc = UIBezierPath(arcCenter: CGPoint(x: size / 2, y: size / 2),
        radius: (ring - strokeWidth) / 2, startAngle: 0, endAngle: .pi * 1.5, clockwise: true)
      arc.lineWidth = strokeWidth
      arc.stroke()
      let head = size * 0.2
      context.cgContext.translateBy(x: size * 0.62 + head / 2, y: size * 0.2 + head / 2)
      context.cgContext.rotate(by: .pi * 100 / 180)
      ShadowListUtilsChevronIcon.path(side: head, strokeWidth: strokeWidth).fill()
    }.withRenderingMode(.alwaysTemplate)
  }
}
