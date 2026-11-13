import ShadowListKit
import UIKit

public final class ShadowListUtilsBoardCardLayout: ShadowListUtilsRowLayout {
  public let card: CGRect
  public let accent: CGRect
  public let title: ShadowListKitTextLayout
  public let titleFrame: CGRect
  public let detail: ShadowListKitTextLayout?
  public let detailFrame: CGRect
  init(width: CGFloat, height: CGFloat, card: CGRect, accent: CGRect, title: ShadowListKitTextLayout,
       titleFrame: CGRect, detail: ShadowListKitTextLayout?, detailFrame: CGRect) {
    self.card = card
    self.accent = accent
    self.title = title
    self.titleFrame = titleFrame
    self.detail = detail
    self.detailFrame = detailFrame
    super.init(width: width, height: height)
  }
}
