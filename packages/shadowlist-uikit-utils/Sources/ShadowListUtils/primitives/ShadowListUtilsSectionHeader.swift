import ShadowListKit
import UIKit

/*
 * The header of a section: a small title with an optional count at the trailing edge. The
 * title is upper cased when the theme's grouped caption case says so.
 */
public struct ShadowListUtilsSectionHeader: ShadowListUtilsRow {
  public let key: String
  public let title: String
  public let count: Int?
  public let theme: ShadowListUtilsTheme
  public let contentVersion: Int

  public init(key: String, title: String, count: Int? = nil, theme: ShadowListUtilsTheme) {
    self.key = key
    self.title = title
    self.count = count
    self.theme = theme
    var hasher = Hasher()
    hasher.combine(title)
    hasher.combine(count)
    contentVersion = hasher.finalize()
  }

  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsSectionHeaderView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let footnote = theme.typography.footnote
    let inset = theme.spacing.lg
    let contentWidth = width - 2 * inset
    let direction = ShadowListUtilsDirection(width: width)
    let countText = count.map { footnote.layout("\($0)", width: contentWidth, maxLines: 1) }
    let countWidth = countText.map { $0.size.width + theme.spacing.sm } ?? 0
    let shown = theme.grouped.captionCase == .upper ? title.uppercased() : title
    let titleText = footnote.with(.semibold).layout(shown, width: contentWidth - countWidth, maxLines: 1)
    let titleFrame = CGRect(CGPoint(x: inset, y: Self.paddingVertical), titleText)
    let countFrame = countText.map { CGRect(CGPoint(x: width - inset - $0.size.width, y: Self.paddingVertical), $0) } ?? .zero
    return ShadowListUtilsSectionHeaderLayout(
      width: width, height: footnote.scaledLineHeight() + 2 * Self.paddingVertical,
      title: titleText, titleFrame: direction.place(titleFrame), count: countText, countFrame: direction.place(countFrame))
  }

  private static let paddingVertical: CGFloat = 10
}

public final class ShadowListUtilsSectionHeaderLayout: ShadowListUtilsRowLayout {
  public let title: ShadowListKitTextLayout
  public let titleFrame: CGRect
  public let count: ShadowListKitTextLayout?
  public let countFrame: CGRect

  init(width: CGFloat, height: CGFloat, title: ShadowListKitTextLayout, titleFrame: CGRect, count: ShadowListKitTextLayout?, countFrame: CGRect) {
    self.title = title
    self.titleFrame = titleFrame
    self.count = count
    self.countFrame = countFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsSectionHeaderView: ShadowListUtilsRowView {
  private let title = ShadowListKitTextView(frame: .zero)
  private let count = ShadowListKitTextView(frame: .zero)

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    accessibilityTraits = .header
    addSubview(title)
    addSubview(count)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsSectionHeader else { return }
    restingBackgroundColor = row.theme.colors.elevated
    accessibilityLabel = row.count.map { "\(row.title), \($0)" } ?? row.title
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsSectionHeader, let layout = layout as? ShadowListUtilsSectionHeaderLayout else { return }
    title.show(layout.title, row.theme.colors.secondaryLabel)
    title.frame = layout.titleFrame
    count.isHidden = layout.count == nil
    count.show(layout.count, row.theme.colors.tertiaryLabel)
    count.frame = layout.countFrame
  }
}
