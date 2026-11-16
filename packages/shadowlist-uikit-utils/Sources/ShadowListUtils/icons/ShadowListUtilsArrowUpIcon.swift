import UIKit

enum ShadowListUtilsArrowUpIcon {
  static func image(size: CGFloat = 20, strokeWidth: CGFloat = 2.2) -> UIImage {
    UIGraphicsImageRenderer(size: CGSize(width: size, height: size)).image { context in
      UIColor.black.setFill()
      let stem = CGRect(x: (size - strokeWidth) / 2, y: size * 0.26, width: strokeWidth, height: size * 0.56)
      UIBezierPath(roundedRect: stem, cornerRadius: strokeWidth / 2).fill()
      let head = size * 0.36
      context.cgContext.translateBy(x: size / 2, y: size * 0.16 + head / 2)
      context.cgContext.rotate(by: -.pi / 4)
      ShadowListUtilsChevronIcon.path(side: head, strokeWidth: strokeWidth).fill()
    }.withRenderingMode(.alwaysTemplate)
  }
}
