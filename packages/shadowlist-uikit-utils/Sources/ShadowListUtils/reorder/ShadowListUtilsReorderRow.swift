import ShadowListKit
import UIKit

/*
 * A contact with a grip at the trailing edge, for a list whose items are dragged into a new
 * order.
 */
public struct ShadowListUtilsReorderRow: ShadowListUtilsRow {
  public let item: ShadowListUtilsContactItem
  public let theme: ShadowListUtilsTheme
  public let labels: ShadowListUtilsReorderLabels
  public let contentVersion: Int

  public init(item: ShadowListUtilsContactItem, theme: ShadowListUtilsTheme, labels: ShadowListUtilsReorderLabels) {
    self.item = item
    self.theme = theme
    self.labels = labels
    contentVersion = item.hashValue
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsReorderRowView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let direction = ShadowListUtilsDirection(width: width)
    let trailing = theme.spacing.lg
    let grip = ShadowListUtilsContactBody.iconSize
    let body = ShadowListUtilsContactBody(item: item, theme: theme, width: width, trailing: trailing + grip)
    let gripFrame = CGRect(x: width - trailing - grip, y: ShadowListUtilsPlatform.roundToPixel((body.height - grip) / 2), width: grip, height: grip)
    return ShadowListUtilsReorderRowLayout(
      width: width, height: body.height, avatarFrame: direction.place(body.avatarFrame), name: body.name,
      nameFrame: direction.place(body.nameFrame), subtitle: body.subtitle, subtitleFrame: direction.place(body.subtitleFrame),
      gripFrame: direction.place(gripFrame), separatorFrame: direction.place(body.separatorFrame))
  }
}

public final class ShadowListUtilsReorderRowLayout: ShadowListUtilsRowLayout {
  public let avatarFrame: CGRect
  public let name: ShadowListKitTextLayout
  public let nameFrame: CGRect
  public let subtitle: ShadowListKitTextLayout?
  public let subtitleFrame: CGRect
  public let gripFrame: CGRect
  public let separatorFrame: CGRect

  init(
    width: CGFloat, height: CGFloat, avatarFrame: CGRect, name: ShadowListKitTextLayout, nameFrame: CGRect,
    subtitle: ShadowListKitTextLayout?, subtitleFrame: CGRect, gripFrame: CGRect, separatorFrame: CGRect
  ) {
    self.avatarFrame = avatarFrame
    self.name = name
    self.nameFrame = nameFrame
    self.subtitle = subtitle
    self.subtitleFrame = subtitleFrame
    self.gripFrame = gripFrame
    self.separatorFrame = separatorFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsReorderRowView: ShadowListUtilsRowView {
  private let avatar = ShadowListUtilsAvatar(size: ShadowListUtilsContactBody.avatarSize, theme: .system)
  private let name = ShadowListKitTextView(frame: .zero)
  private let subtitle = ShadowListKitTextView(frame: .zero)
  private let grip = ShadowListUtilsIconView(kind: .grip)
  private let separator = UIView()

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    [avatar, name, subtitle, grip, separator].forEach(addSubview)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsReorderRow else { return }
    let colors = row.theme.colors
    restingBackgroundColor = colors.background
    separator.backgroundColor = colors.separator
    grip.color = colors.tertiaryLabel
    avatar.theme = row.theme
    avatar.show(name: row.item.name, imageUrl: row.item.avatarUrl, color: row.item.avatarColor)
    accessibilityLabel = row.item.accessibilityLabel
    accessibilityHint = row.labels.dragHint
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsReorderRow, let layout = layout as? ShadowListUtilsReorderRowLayout else { return }
    avatar.frame = layout.avatarFrame
    name.show(layout.name, row.theme.colors.label)
    name.frame = layout.nameFrame
    subtitle.isHidden = layout.subtitle == nil
    subtitle.show(layout.subtitle, row.theme.colors.secondaryLabel)
    subtitle.frame = layout.subtitleFrame
    grip.frame = layout.gripFrame
    separator.frame = layout.separatorFrame
  }
}
