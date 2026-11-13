import UIKit

final class ShadowListUtilsChipClearButton: UIButton {
  override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
    bounds.insetBy(dx: -8, dy: -8).contains(point)
  }
}
