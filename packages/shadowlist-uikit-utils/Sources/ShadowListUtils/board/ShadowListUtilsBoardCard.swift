import ShadowListKit
import UIKit

/*
 * A board card's title, optional label and detail, with an optional color bar. Its cell stays
 * transparent and the column paints behind the rounded card, including while the card lifts.
 */
public struct ShadowListUtilsBoardCard: ShadowListUtilsRow {
  public let key: String
  public let title: String
  public let label: String?
  public let detail: String?
  public let color: UIColor?
  public let accessibilityLabel: String?
  public let theme: ShadowListUtilsTheme

  public init(key: String, title: String, label: String? = nil, detail: String? = nil, color: UIColor? = nil,
              accessibilityLabel: String? = nil, theme: ShadowListUtilsTheme = .system) {
    self.key = key
    self.title = title
    self.label = label
    self.detail = detail
    self.color = color
    self.accessibilityLabel = accessibilityLabel
    self.theme = theme
  }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsBoardCardView.self }
  public var contentVersion: Int {
    var hasher = Hasher()
    hasher.combine(title)
    hasher.combine(label)
    hasher.combine(detail)
    hasher.combine(color?.hash)
    hasher.combine(accessibilityLabel)
    return hasher.finalize()
  }
  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let inset = theme.spacing.md
    let textWidth = max(1, width - inset * 4)
    let title = theme.typography.headline.layout(self.title, width: textWidth, maxLines: 0)
    let meta = [label, detail].compactMap { $0 }.joined(separator: " · ")
    let detail = meta.isEmpty ? nil : theme.typography.footnote.layout(meta, width: textWidth, maxLines: 0)
    let accent = color == nil ? 0 : theme.spacing.xs
    let height = inset * 3 + accent + title.size.height + (detail.map { theme.spacing.sm + $0.size.height } ?? 0)
    return ShadowListUtilsBoardCardLayout(width: width, height: height,
      card: CGRect(x: inset, y: 0, width: max(0, width - inset * 2), height: height - inset),
      accent: CGRect(x: inset * 2, y: inset, width: textWidth, height: accent), title: title,
      titleFrame: CGRect(x: inset * 2, y: inset + accent, width: title.size.width, height: title.size.height), detail: detail,
      detailFrame: CGRect(x: inset * 2, y: inset + accent + title.size.height + theme.spacing.sm,
                          width: detail?.size.width ?? 0, height: detail?.size.height ?? 0))
  }
}
