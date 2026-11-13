import ShadowListKit
import UIKit

public final class ShadowListUtilsTreeRowLayout: ShadowListUtilsRowLayout {
  public let name: ShadowListKitTextLayout
  public let nameFrame: CGRect
  public let chevronFrame: CGRect
  public let iconFrame: CGRect
  public let count: ShadowListKitTextLayout?
  public let countFrame: CGRect

  init(width: CGFloat, height: CGFloat, name: ShadowListKitTextLayout, nameFrame: CGRect, chevronFrame: CGRect,
       iconFrame: CGRect, count: ShadowListKitTextLayout?, countFrame: CGRect) {
    self.name = name
    self.nameFrame = nameFrame
    self.chevronFrame = chevronFrame
    self.iconFrame = iconFrame
    self.count = count
    self.countFrame = countFrame
    super.init(width: width, height: height)
  }
}
