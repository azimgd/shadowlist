import UIKit

enum ShadowListUtilsSparkleIcon {
  static func image(size: CGFloat = 20) -> UIImage {
    UIGraphicsImageRenderer(size: CGSize(width: size, height: size)).image { context in
      UIColor.black.setFill()
      let side = size * 0.46
      let path = UIBezierPath(roundedRect: CGRect(x: -side / 2, y: -side / 2, width: side, height: side), cornerRadius: side * 0.14)
      context.cgContext.translateBy(x: size / 2, y: size / 2)
      path.fill()
      context.cgContext.rotate(by: .pi / 4)
      path.fill()
    }.withRenderingMode(.alwaysTemplate)
  }
}
