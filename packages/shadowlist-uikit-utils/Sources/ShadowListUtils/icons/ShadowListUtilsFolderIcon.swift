import UIKit

public enum ShadowListUtilsFolderIcon {
  public static func image(size: CGFloat = 18) -> UIImage {
    UIGraphicsImageRenderer(size: CGSize(width: size, height: size * 0.82)).image { _ in
      UIColor.black.setFill()
      UIBezierPath(roundedRect: CGRect(x: 0, y: 0, width: size * 0.44, height: size * 0.28),
        byRoundingCorners: [.topLeft, .topRight], cornerRadii: CGSize(width: 2.5, height: 2.5)).fill()
      UIBezierPath(roundedRect: CGRect(x: 0, y: size * 0.16, width: size, height: size * 0.66), cornerRadius: 3).fill()
    }.withRenderingMode(.alwaysTemplate)
  }
}
