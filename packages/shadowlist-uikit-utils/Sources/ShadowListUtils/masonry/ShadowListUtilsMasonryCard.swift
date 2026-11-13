import ShadowListKit
import UIKit

/*
 * A card of a masonry grid: an image at its own aspect with an optional title of two lines
 * under it, and of four at a large text size.
 */
public struct ShadowListUtilsMasonryCard: ShadowListUtilsRow {
  public let item: ShadowListUtilsMasonryItem
  public let theme: ShadowListUtilsTheme
  public let labels: ShadowListUtilsMasonryLabels
  public let contentVersion: Int

  public init(item: ShadowListUtilsMasonryItem, theme: ShadowListUtilsTheme, labels: ShadowListUtilsMasonryLabels) {
    self.item = item
    self.theme = theme
    self.labels = labels
    contentVersion = item.hashValue
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsMasonryCardView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let direction = ShadowListUtilsDirection(width: width)
    let imageWidth = max(0, width - 2 * Self.paddingHorizontal)
    let aspectRatio = item.image.height > 0 ? item.image.width / item.image.height : 1
    let imageHeight = aspectRatio > 0 ? ShadowListUtilsPlatform.roundToPixel(imageWidth / aspectRatio) : imageWidth
    let imageFrame = CGRect(x: Self.paddingHorizontal, y: 0, width: imageWidth, height: imageHeight)
    var bottom = imageFrame.maxY + theme.spacing.sm
    var title: ShadowListKitTextLayout?
    var titleFrame = CGRect.zero
    if let text = item.title {
      let lines = ShadowListUtilsPlatform.isLargeText ? Self.largeTextTitleLines : Self.titleLines
      let typeset = theme.typography.subhead.layout(text, width: imageWidth - 2 * theme.spacing.xs, maxLines: lines)
      titleFrame = CGRect(CGPoint(x: Self.paddingHorizontal + theme.spacing.xs, y: bottom), typeset)
      title = typeset
      bottom += typeset.size.height
    }
    return ShadowListUtilsMasonryCardLayout(
      width: width, height: bottom + theme.spacing.md, imageFrame: direction.place(imageFrame), title: title,
      titleFrame: direction.place(titleFrame))
  }

  static let paddingHorizontal: CGFloat = 6
  private static let titleLines = 2
  private static let largeTextTitleLines = 4
}

public final class ShadowListUtilsMasonryCardLayout: ShadowListUtilsRowLayout {
  public let imageFrame: CGRect
  public let title: ShadowListKitTextLayout?
  public let titleFrame: CGRect

  init(width: CGFloat, height: CGFloat, imageFrame: CGRect, title: ShadowListKitTextLayout?, titleFrame: CGRect) {
    self.imageFrame = imageFrame
    self.title = title
    self.titleFrame = titleFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsMasonryCardView: ShadowListUtilsRowView {
  private let image = ShadowListUtilsImageView()
  private let title = ShadowListKitTextView(frame: .zero)

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    accessibilityTraits = .image
    addSubview(image)
    addSubview(title)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsMasonryCard else { return }
    restingBackgroundColor = row.theme.colors.background
    image.cornerRadius = row.theme.radius.md
    image.placeholderColor = row.theme.colors.elevated2
    image.url = row.item.image.url
    accessibilityLabel = row.item.image.alt ?? row.item.title ?? row.labels.image
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsMasonryCard, let layout = layout as? ShadowListUtilsMasonryCardLayout else { return }
    image.frame = layout.imageFrame
    title.isHidden = layout.title == nil
    title.show(layout.title, row.theme.colors.label)
    title.frame = layout.titleFrame
  }
}
