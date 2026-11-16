import UIKit

public enum ShadowListUtilsCopyIcon {
  public static func image(size: CGFloat = 20, strokeWidth: CGFloat = 1.6) -> UIImage {
    UIGraphicsImageRenderer(size: CGSize(width: size, height: size)).image { _ in
      UIColor.black.setStroke()
      let sheet = size * 0.5
      for offset in [0.16, 0.34] {
        let path = UIBezierPath(roundedRect: CGRect(x: size * offset, y: size * offset,
          width: sheet, height: sheet).insetBy(dx: strokeWidth / 2, dy: strokeWidth / 2),
          cornerRadius: sheet * 0.22 - strokeWidth / 2)
        path.lineWidth = strokeWidth
        path.stroke()
      }
    }.withRenderingMode(.alwaysTemplate)
  }
}
