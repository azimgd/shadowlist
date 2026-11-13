import ShadowListKit
import UIKit

/*
 * The frames and typeset text of a poll option.
 */
public final class ShadowListUtilsPollRowLayout: ShadowListUtilsRowLayout {
  public let selectionFrame: CGRect
  public let label: ShadowListKitTextLayout
  public let labelFrame: CGRect
  public let percent: ShadowListKitTextLayout
  public let percentFrame: CGRect
  public let barFrame: CGRect
  public let barFillFrame: CGRect

  init(
    width: CGFloat, height: CGFloat, selectionFrame: CGRect, label: ShadowListKitTextLayout,
    labelFrame: CGRect, percent: ShadowListKitTextLayout, percentFrame: CGRect, barFrame: CGRect, barFillFrame: CGRect
  ) {
    self.selectionFrame = selectionFrame
    self.label = label
    self.labelFrame = labelFrame
    self.percent = percent
    self.percentFrame = percentFrame
    self.barFrame = barFrame
    self.barFillFrame = barFillFrame
    super.init(width: width, height: height)
  }
}
