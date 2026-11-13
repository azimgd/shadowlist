import ShadowListKit
import UIKit

/*
 * Shows an activity from its precomputed layout. Reuse resets its unread state and accessibility.
 */
public final class ShadowListUtilsActivityRowView: ShadowListUtilsRowView {
  private let avatar = ShadowListUtilsAvatar(size: ShadowListUtilsActivityRow.avatarSize, theme: .system)
  private let unreadDot = UIView()
  private let title = ShadowListKitTextView(frame: .zero)
  private let text = ShadowListKitTextView(frame: .zero)
  private let time = ShadowListKitTextView(frame: .zero)

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    [avatar, unreadDot, title, text, time].forEach(addSubview)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsActivityRow else { return }
    restingBackgroundColor = row.theme.colors.background
    pressedBackgroundColor = row.interactive ? row.theme.colors.fill : nil
    unreadDot.backgroundColor = row.theme.colors.accent
    unreadDot.isHidden = row.item.read
    avatar.theme = row.theme
    avatar.show(name: row.item.actor.name, imageUrl: row.item.actor.avatarUrl, color: row.item.actor.avatarColor)
    accessibilityLabel = row.accessibilityLabel
    accessibilityTraits = row.interactive ? .button : []
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsActivityRow, let layout = layout as? ShadowListUtilsActivityRowLayout else { return }
    avatar.frame = layout.avatarFrame
    unreadDot.frame = layout.unreadDotFrame
    unreadDot.layer.cornerRadius = layout.unreadDotFrame.width / 2
    title.show(layout.title, row.theme.colors.label)
    title.frame = layout.titleFrame
    text.isHidden = layout.text == nil
    if let shown = layout.text { text.show(shown, row.theme.colors.secondaryLabel) }
    text.frame = layout.textFrame
    time.show(layout.time, row.theme.colors.tertiaryLabel)
    time.frame = layout.timeFrame
  }
}
