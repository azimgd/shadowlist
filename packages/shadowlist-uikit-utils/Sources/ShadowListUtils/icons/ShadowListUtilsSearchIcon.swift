import UIKit

enum ShadowListUtilsSearchIcon {
  static func image(size: CGFloat = 17, strokeWidth: CGFloat = 1.8) -> UIImage {
    UIGraphicsImageRenderer(size: CGSize(width: size, height: size)).image { context in
      UIColor.black.setStroke()
      UIColor.black.setFill()
      let lens = size * 0.62
      let handle = size * 0.34
      let circle = UIBezierPath(ovalIn: CGRect(x: size * 0.06, y: size * 0.06,
        width: lens, height: lens).insetBy(dx: strokeWidth / 2, dy: strokeWidth / 2))
      circle.lineWidth = strokeWidth
      circle.stroke()
      context.cgContext.translateBy(x: size * 0.06 + lens * 0.85 - strokeWidth / 2,
        y: size * 0.06 + lens * 0.85 + handle * 0.15 + strokeWidth / 2)
      context.cgContext.rotate(by: .pi / 4)
      UIBezierPath(roundedRect: CGRect(x: 0, y: -strokeWidth / 2, width: handle, height: strokeWidth),
        cornerRadius: strokeWidth).fill()
    }.withRenderingMode(.alwaysTemplate)
  }
}
