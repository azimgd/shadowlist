import ShadowListKit
import UIKit

/*
 * A tile of a reorder grid: a colored swatch with the item's label on it and its title under
 * it. aspectRatio, when set, is the swatch's width over its height in place of the item's own,
 * and keeps the title to one line to make every tile the same height. The label keeps its size
 * whatever the text size setting.
 */
public struct ShadowListUtilsReorderTile: ShadowListUtilsRow {
  public let item: ShadowListUtilsReorderTileItem
  public let aspectRatio: CGFloat?
  public let theme: ShadowListUtilsTheme
  public let labels: ShadowListUtilsReorderLabels
  public let contentVersion: Int

  public init(
    item: ShadowListUtilsReorderTileItem,
    aspectRatio: CGFloat?,
    theme: ShadowListUtilsTheme,
    labels: ShadowListUtilsReorderLabels
  ) {
    self.item = item
    self.aspectRatio = aspectRatio
    self.theme = theme
    self.labels = labels
    var hasher = Hasher()
    hasher.combine(item)
    hasher.combine(aspectRatio)
    contentVersion = hasher.finalize()
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsReorderTileView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let spacing = theme.spacing
    let direction = ShadowListUtilsDirection(width: width)
    let swatchWidth = max(0, width - 2 * Self.paddingHorizontal)
    let ratio = aspectRatio ?? item.aspectRatio ?? 1
    let swatchFrame = CGRect(
      x: Self.paddingHorizontal, y: 0, width: swatchWidth,
      height: ratio > 0 ? ShadowListUtilsPlatform.roundToPixel(swatchWidth / ratio) : swatchWidth)
    var label: ShadowListKitTextLayout?
    var labelFrame = CGRect.zero
    if let text = item.label {
      let typeset = theme.typography.title2.layout(text, width: swatchWidth, maxLines: 1, scaled: false)
      labelFrame = CGRect(
        x: ShadowListUtilsPlatform.roundToPixel(swatchFrame.midX - typeset.size.width / 2),
        y: ShadowListUtilsPlatform.roundToPixel(swatchFrame.midY - typeset.size.height / 2),
        width: typeset.size.width, height: typeset.size.height)
      label = typeset
    }
    let lines = aspectRatio != nil ? 1 : ShadowListUtilsPlatform.isLargeText ? Self.largeTextTitleLines : Self.titleLines
    let title = theme.typography.footnote.layout(item.title, width: swatchWidth - 2 * spacing.xs, maxLines: lines)
    let titleFrame = CGRect(CGPoint(x: Self.paddingHorizontal + spacing.xs, y: swatchFrame.maxY + spacing.xs), title)
    return ShadowListUtilsReorderTileLayout(
      width: width, height: titleFrame.maxY + spacing.md, swatchFrame: direction.place(swatchFrame), label: label,
      labelFrame: direction.place(labelFrame), title: title, titleFrame: direction.place(titleFrame))
  }

  private static let paddingHorizontal: CGFloat = 6
  private static let titleLines = 2
  private static let largeTextTitleLines = 4
}

public final class ShadowListUtilsReorderTileLayout: ShadowListUtilsRowLayout {
  public let swatchFrame: CGRect
  public let label: ShadowListKitTextLayout?
  public let labelFrame: CGRect
  public let title: ShadowListKitTextLayout
  public let titleFrame: CGRect

  init(
    width: CGFloat, height: CGFloat, swatchFrame: CGRect, label: ShadowListKitTextLayout?, labelFrame: CGRect,
    title: ShadowListKitTextLayout, titleFrame: CGRect
  ) {
    self.swatchFrame = swatchFrame
    self.label = label
    self.labelFrame = labelFrame
    self.title = title
    self.titleFrame = titleFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsReorderTileView: ShadowListUtilsRowView {
  private let swatch = UIView()
  private let label = ShadowListKitTextView(frame: .zero)
  private let title = ShadowListKitTextView(frame: .zero)

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    [swatch, label, title].forEach(addSubview)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsReorderTile else { return }
    let item = row.item
    restingBackgroundColor = row.theme.colors.background
    swatch.backgroundColor = item.color ?? row.theme.colors.avatarColor(for: item.label ?? item.title)
    swatch.layer.cornerRadius = row.theme.radius.md
    accessibilityLabel = item.label.map { "\($0), \(item.title)" } ?? item.title
    accessibilityHint = row.labels.dragHint
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsReorderTile, let layout = layout as? ShadowListUtilsReorderTileLayout else { return }
    swatch.frame = layout.swatchFrame
    label.isHidden = layout.label == nil
    label.show(layout.label, .white)
    label.frame = layout.labelFrame
    title.show(layout.title, row.theme.colors.label)
    title.frame = layout.titleFrame
  }
}
