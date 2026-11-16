import ShadowListKit
import UIKit

public final class ShadowListUtilsBoardCardLayout: ShadowListUtilsRowLayout {
  public let card: CGRect
  public let accent: CGRect
  public let title: ShadowListKitTextLayout
  public let titleFrame: CGRect
  public let detail: ShadowListKitTextLayout?
  public let detailFrame: CGRect
  public let due: ShadowListKitTextLayout?
  public let dueFrame: CGRect
  public let dotFrame: CGRect
  init(width: CGFloat, height: CGFloat, card: CGRect, accent: CGRect, title: ShadowListKitTextLayout,
       titleFrame: CGRect, detail: ShadowListKitTextLayout?, detailFrame: CGRect, due: ShadowListKitTextLayout?,
       dueFrame: CGRect, dotFrame: CGRect) {
    self.card = card
    self.accent = accent
    self.title = title
    self.titleFrame = titleFrame
    self.detail = detail
    self.detailFrame = detailFrame
    self.due = due
    self.dueFrame = dueFrame
    self.dotFrame = dotFrame
    super.init(width: width, height: height)
  }
}
