import ShadowListKit
import UIKit

/*
 * The frames and typeset text of an activity row.
 */
public final class ShadowListUtilsActivityRowLayout: ShadowListUtilsRowLayout {
  public let avatarFrame: CGRect
  public let unreadDotFrame: CGRect
  public let title: ShadowListKitTextLayout
  public let titleFrame: CGRect
  public let text: ShadowListKitTextLayout?
  public let textFrame: CGRect
  public let time: ShadowListKitTextLayout
  public let timeFrame: CGRect

  init(
    width: CGFloat, height: CGFloat, avatarFrame: CGRect, unreadDotFrame: CGRect,
    title: ShadowListKitTextLayout, titleFrame: CGRect, text: ShadowListKitTextLayout?, textFrame: CGRect,
    time: ShadowListKitTextLayout, timeFrame: CGRect
  ) {
    self.avatarFrame = avatarFrame
    self.unreadDotFrame = unreadDotFrame
    self.title = title
    self.titleFrame = titleFrame
    self.text = text
    self.textFrame = textFrame
    self.time = time
    self.timeFrame = timeFrame
    super.init(width: width, height: height)
  }
}
