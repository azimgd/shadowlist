import ShadowListKit
import UIKit

/*
 * An activity row: the actor's avatar, their action, optional text and its age. Large text lets
 * the title wrap and gives the text four lines. Time is written once when the row is made.
 */
public struct ShadowListUtilsActivityRow: ShadowListUtilsRow {
  public let item: ShadowListUtilsActivityItem
  public let interactive: Bool
  public let theme: ShadowListUtilsTheme
  public let labels: ShadowListUtilsActivityLabels
  public let time: String
  public let contentVersion: Int
  let accessibilityLabel: String

  public init(
    item: ShadowListUtilsActivityItem, interactive: Bool = false,
    theme: ShadowListUtilsTheme, labels: ShadowListUtilsActivityLabels = ShadowListUtilsActivityLabels(),
    formatTime: ((Double) -> String)? = nil
  ) {
    self.item = item
    self.interactive = interactive
    self.theme = theme
    self.labels = labels
    time = formatTime?(item.createdAtMs) ?? labels.time.format(dateMs: item.createdAtMs, nowMs: ShadowListUtilsRelativeTimeLabels.nowMs)
    accessibilityLabel = [item.read ? nil : labels.unread, "\(item.actor.name) \(item.action)", item.text, time]
      .compactMap { $0 }.filter { !$0.isEmpty }.joined(separator: ", ")
    var hasher = Hasher()
    hasher.combine(item)
    hasher.combine(interactive)
    hasher.combine(accessibilityLabel)
    contentVersion = hasher.finalize()
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsActivityRowView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let spacing = theme.spacing
    let typography = theme.typography
    let direction = ShadowListUtilsDirection(width: width)
    let top = spacing.md
    let avatarFrame = CGRect(x: spacing.lg, y: top, width: Self.avatarSize, height: Self.avatarSize)
    let bodyX = avatarFrame.maxX + spacing.md
    let bodyWidth = max(1, width - bodyX - spacing.lg)
    let time = typography.footnote.layout(self.time, width: bodyWidth / 3, maxLines: 1)
    let timeFrame = CGRect(CGPoint(x: width - spacing.lg - time.size.width, y: top), time)
    let titleWidth = max(1, bodyWidth - time.size.width - spacing.sm)
    let title = typography.subhead.with(.semibold).layout(
      "\(item.actor.name) \(item.action)", width: titleWidth, maxLines: ShadowListUtilsPlatform.isLargeText ? 0 : 1)
    let titleFrame = CGRect(CGPoint(x: bodyX, y: top), title)
    let text = item.text.flatMap { $0.isEmpty ? nil : typography.footnote.layout(
      $0, width: titleWidth, maxLines: ShadowListUtilsPlatform.isLargeText ? 4 : 2) }
    let textFrame = text.map { CGRect(CGPoint(x: bodyX, y: titleFrame.maxY + spacing.xxs), $0) } ?? .zero
    let unreadDotFrame = CGRect(
      x: spacing.xs, y: top + (Self.avatarSize - spacing.sm) / 2, width: spacing.sm, height: spacing.sm)
    return ShadowListUtilsActivityRowLayout(
      width: width, height: max(avatarFrame.maxY, max(titleFrame.maxY, textFrame.maxY)) + spacing.md,
      avatarFrame: direction.place(avatarFrame), unreadDotFrame: direction.place(unreadDotFrame),
      title: title, titleFrame: direction.place(titleFrame), text: text, textFrame: direction.place(textFrame),
      time: time, timeFrame: direction.place(timeFrame))
  }

  static let avatarSize: CGFloat = 40
}
