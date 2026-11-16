import UIKit

public enum ShadowListUtilsPlusIcon {
  public static func image(size: CGFloat = 20, strokeWidth: CGFloat = 2) -> UIImage {
    UIGraphicsImageRenderer(size: CGSize(width: size, height: size)).image { _ in
      UIColor.black.setFill()
      let bar = size * 0.56
      UIBezierPath(roundedRect: CGRect(x: (size - bar) / 2, y: (size - strokeWidth) / 2,
        width: bar, height: strokeWidth), cornerRadius: strokeWidth).fill()
      UIBezierPath(roundedRect: CGRect(x: (size - strokeWidth) / 2, y: (size - bar) / 2,
        width: strokeWidth, height: bar), cornerRadius: strokeWidth).fill()
    }.withRenderingMode(.alwaysTemplate)
  }
}
