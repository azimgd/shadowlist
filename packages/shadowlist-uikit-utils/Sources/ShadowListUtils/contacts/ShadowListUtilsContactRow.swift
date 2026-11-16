import ShadowListKit
import UIKit

/*
 * A contact: an avatar, a name and an optional subtitle of one line each, with a chevron at the
 * trailing edge when showsChevron is set. At a large text size both lines wrap. The row
 * highlights while pressed in a list that selects.
 */
public struct ShadowListUtilsContactRow: ShadowListUtilsRow {
  public let item: ShadowListUtilsContactItem
  public let showsChevron: Bool
  public let theme: ShadowListUtilsTheme
  public let contentVersion: Int

  public init(item: ShadowListUtilsContactItem, showsChevron: Bool, theme: ShadowListUtilsTheme) {
    self.item = item
    self.showsChevron = showsChevron
    self.theme = theme
    var hasher = Hasher()
    hasher.combine(item)
    hasher.combine(showsChevron)
    contentVersion = hasher.finalize()
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsContactRowView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let direction = ShadowListUtilsDirection(width: width)
    let trailing = theme.spacing.md
    let accessory = showsChevron ? ShadowListUtilsContactBody.iconSize : 0
    let body = ShadowListUtilsContactBody(item: item, theme: theme, width: width, trailing: trailing + accessory)
    let chevronFrame = showsChevron
      ? CGRect(
        x: width - trailing - accessory, y: ShadowListUtilsPlatform.roundToPixel((body.height - accessory) / 2), width: accessory,
        height: accessory)
      : .zero
    return ShadowListUtilsContactRowLayout(
      width: width, height: body.height, avatarFrame: direction.place(body.avatarFrame), name: body.name,
      nameFrame: direction.place(body.nameFrame), subtitle: body.subtitle, subtitleFrame: direction.place(body.subtitleFrame),
      chevronFrame: direction.place(chevronFrame), separatorFrame: direction.place(body.separatorFrame))
  }
}

/*
 * The frames a contact row and a reorder row share, for a layout that runs from left to right:
 * the avatar, the two lines of text beside it and the hairline under them. trailing is the
 * space the row keeps free after the text.
 */
struct ShadowListUtilsContactBody {
  let avatarFrame: CGRect
  let name: ShadowListKitTextLayout
  let nameFrame: CGRect
  let subtitle: ShadowListKitTextLayout?
  let subtitleFrame: CGRect
  let separatorFrame: CGRect
  let height: CGFloat

  init(item: ShadowListUtilsContactItem, theme: ShadowListUtilsTheme, width: CGFloat, trailing: CGFloat) {
    let spacing = theme.spacing
    let textX = spacing.lg + Self.avatarSize + spacing.md
    let textWidth = max(0, width - textX - trailing)
    let lines = ShadowListUtilsPlatform.isLargeText ? 0 : 1
    let name = theme.typography.body.layout(item.name, width: textWidth, maxLines: lines)
    let subtitle = item.subtitle.flatMap { $0.isEmpty ? nil : theme.typography.subhead.layout($0, width: textWidth, maxLines: lines) }
    let nameHeight = name.size.height
    let textHeight = nameHeight + (subtitle.map { Self.subtitleGap + $0.size.height } ?? 0)
    let height = max(Self.avatarSize, textHeight) + 2 * spacing.md
    let textY = (height - textHeight) / 2
    let hairline = ShadowListUtilsPlatform.hairline
    self.name = name
    self.subtitle = subtitle
    self.height = height
    avatarFrame = CGRect(
      x: spacing.lg, y: ShadowListUtilsPlatform.roundToPixel((height - Self.avatarSize) / 2), width: Self.avatarSize, height: Self.avatarSize)
    nameFrame = CGRect(CGPoint(x: textX, y: textY), name)
    subtitleFrame = subtitle.map { CGRect(CGPoint(x: textX, y: textY + nameHeight + Self.subtitleGap), $0) } ?? .zero
    separatorFrame = CGRect(x: theme.rowInset, y: height - hairline, width: max(0, width - theme.rowInset), height: hairline)
  }

  static let avatarSize: CGFloat = 40
  static let iconSize: CGFloat = 20
  private static let subtitleGap: CGFloat = 1
}

public final class ShadowListUtilsContactRowLayout: ShadowListUtilsRowLayout {
  public let avatarFrame: CGRect
  public let name: ShadowListKitTextLayout
  public let nameFrame: CGRect
  public let subtitle: ShadowListKitTextLayout?
  public let subtitleFrame: CGRect
  public let chevronFrame: CGRect
  public let separatorFrame: CGRect

  init(
    width: CGFloat, height: CGFloat, avatarFrame: CGRect, name: ShadowListKitTextLayout, nameFrame: CGRect,
    subtitle: ShadowListKitTextLayout?, subtitleFrame: CGRect, chevronFrame: CGRect, separatorFrame: CGRect
  ) {
    self.avatarFrame = avatarFrame
    self.name = name
    self.nameFrame = nameFrame
    self.subtitle = subtitle
    self.subtitleFrame = subtitleFrame
    self.chevronFrame = chevronFrame
    self.separatorFrame = separatorFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsContactRowView: ShadowListUtilsRowView {
  private let avatar = ShadowListUtilsAvatar(size: ShadowListUtilsContactBody.avatarSize, theme: .system)
  private let name = ShadowListKitTextView(frame: .zero)
  private let subtitle = ShadowListKitTextView(frame: .zero)
  private let chevron = ShadowListUtilsIconView(kind: .chevron)
  private let separator = UIView()

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    [avatar, name, subtitle, chevron, separator].forEach(addSubview)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsContactRow else { return }
    let colors = row.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = colors.fill
    separator.backgroundColor = colors.separator
    chevron.color = colors.tertiaryLabel
    chevron.isHidden = !row.showsChevron
    avatar.theme = row.theme
    avatar.show(name: row.item.name, imageUrl: row.item.avatarUrl, color: row.item.avatarColor)
    accessibilityLabel = row.item.accessibilityLabel
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsContactRow, let layout = layout as? ShadowListUtilsContactRowLayout else { return }
    avatar.frame = layout.avatarFrame
    name.show(layout.name, row.theme.colors.label)
    name.frame = layout.nameFrame
    subtitle.isHidden = layout.subtitle == nil
    subtitle.show(layout.subtitle, row.theme.colors.secondaryLabel)
    subtitle.frame = layout.subtitleFrame
    chevron.frame = layout.chevronFrame
    separator.frame = layout.separatorFrame
  }
}
