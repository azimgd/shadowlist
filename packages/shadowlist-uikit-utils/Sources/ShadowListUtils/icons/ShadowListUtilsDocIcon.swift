import UIKit

public enum ShadowListUtilsDocIcon {
  public static func image(size: CGFloat = 18, strokeWidth: CGFloat = 1.6) -> UIImage {
    UIGraphicsImageRenderer(size: CGSize(width: size, height: size)).image { _ in
      UIColor.black.setStroke()
      UIColor.black.setFill()
      let width = size * 0.72
      let height = size * 0.9
      let page = CGRect(x: (size - width) / 2, y: (size - height) / 2, width: width, height: height)
      let border = UIBezierPath(roundedRect: page.insetBy(dx: strokeWidth / 2, dy: strokeWidth / 2), cornerRadius: 2.5 - strokeWidth / 2)
      border.lineWidth = strokeWidth
      border.stroke()
      let lineWidth = width - 2 * (width * 0.18 + strokeWidth)
      let top = (size - (2 * strokeWidth + height * 0.16)) / 2
      for index in 0..<2 {
        UIBezierPath(roundedRect: CGRect(x: page.minX + width * 0.18 + strokeWidth,
          y: top + CGFloat(index) * (strokeWidth + height * 0.16), width: lineWidth * (index == 0 ? 1 : 0.7),
          height: strokeWidth), cornerRadius: strokeWidth / 2).fill()
      }
    }.withRenderingMode(.alwaysTemplate)
  }
}
