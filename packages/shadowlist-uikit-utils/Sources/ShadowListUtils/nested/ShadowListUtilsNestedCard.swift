import ShadowListKit
import UIKit

/*
 * A card of the horizontal list in a nested row: an image with a title of two lines under it.
 * The row sits in a horizontal list: layout(width:) takes the list's height, and the layout's
 * width is the card's size along the list with its leading margin.
 */
public struct ShadowListUtilsNestedCard: ShadowListUtilsRow {
  public let item: ShadowListUtilsNestedCardItem
  public let theme: ShadowListUtilsTheme
  public let contentVersion: Int

  public init(item: ShadowListUtilsNestedCardItem, theme: ShadowListUtilsTheme) {
    self.item = item
    self.theme = theme
    contentVersion = item.hashValue
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsNestedCardView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let leading = theme.spacing.lg
    let along = leading + Self.imageSize.width
    let direction = ShadowListUtilsDirection(width: along)
    let imageFrame = CGRect(origin: CGPoint(x: leading, y: 0), size: Self.imageSize)
    let title = theme.typography.subhead.layout(item.title, width: Self.imageSize.width, maxLines: Self.titleLines)
    let titleFrame = CGRect(CGPoint(x: leading, y: imageFrame.maxY + theme.spacing.sm), title)
    return ShadowListUtilsNestedCardLayout(
      width: along, height: titleFrame.maxY, imageFrame: direction.place(imageFrame), title: title,
      titleFrame: direction.place(titleFrame))
  }

  /*
   * How tall a card is: its image, the gap under it and the lines of its title at the reader's
   * text size. A row's list of cards is this tall.
   */
  static func height(_ theme: ShadowListUtilsTheme) -> CGFloat {
    imageSize.height + theme.spacing.sm + CGFloat(titleLines) * theme.typography.subhead.scaledLineHeight()
  }

  static let imageSize = CGSize(width: 180, height: 220)
  private static let titleLines = 2
}

public final class ShadowListUtilsNestedCardLayout: ShadowListUtilsRowLayout {
  public let imageFrame: CGRect
  public let title: ShadowListKitTextLayout
  public let titleFrame: CGRect

  init(width: CGFloat, height: CGFloat, imageFrame: CGRect, title: ShadowListKitTextLayout, titleFrame: CGRect) {
    self.imageFrame = imageFrame
    self.title = title
    self.titleFrame = titleFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsNestedCardView: ShadowListUtilsRowView {
  private let image = ShadowListUtilsImageView()
  private let title = ShadowListKitTextView(frame: .zero)

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    addSubview(image)
    addSubview(title)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsNestedCard else { return }
    restingBackgroundColor = row.theme.colors.background
    image.cornerRadius = row.theme.radius.md
    image.placeholderColor = row.theme.colors.elevated2
    image.url = row.item.imageUrl
    accessibilityLabel = row.item.imageAlt.map { "\(row.item.title), \($0)" } ?? row.item.title
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsNestedCard, let layout = layout as? ShadowListUtilsNestedCardLayout else { return }
    image.frame = layout.imageFrame
    title.show(layout.title, row.theme.colors.label)
    title.frame = layout.titleFrame
  }
}
