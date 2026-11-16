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
    let border = ShadowListUtilsTheme.hairline
    let accent: CGFloat = color == nil ? 0 : Self.accentWidth
    let textX = inset * 2 + accent + border
    let textRight = width - inset * 2 - border
    let textWidth = max(1, textRight - textX)
    let title = theme.typography.subhead.layout(self.title, width: textWidth, maxLines: 0)
    // The detail's digits are all one width.
    let dueFont = UIFont.monospacedDigitSystemFont(ofSize: theme.typography.caption.fontSize * ShadowListUtilsPlatform.textScale, weight: .regular)
    let due = detail.map {
      theme.typography.caption.layout(NSAttributedString(string: $0, attributes: [.font: dueFont]), width: textWidth / 2, maxLines: 1)
    }
    let dueWidth = due?.size.width ?? 0
    let dotSize: CGFloat = color == nil ? 0 : Self.dotSize
    let dotGap: CGFloat = color == nil ? 0 : theme.spacing.xs
    let detail = label.map {
      theme.typography.caption.with(.semibold).layout(
        $0, width: max(1, textWidth - dueWidth - dotSize - dotGap - theme.spacing.xs), maxLines: 1)
    }
    let top = border + inset
    let metaTop = top + title.size.height + theme.spacing.xs
    let metaHeight = max(detail?.size.height ?? 0, due?.size.height ?? 0)
    let cardHeight = 2 * border + 2 * inset + title.size.height + (detail != nil || due != nil ? theme.spacing.xs + metaHeight : 0)
    return ShadowListUtilsBoardCardLayout(
      width: width, height: cardHeight + theme.spacing.sm,
      card: CGRect(x: inset, y: 0, width: max(0, width - inset * 2), height: cardHeight),
      accent: CGRect(x: inset + border, y: border, width: accent, height: cardHeight - 2 * border),
      title: title, titleFrame: CGRect(x: textX, y: top, width: title.size.width, height: title.size.height),
      detail: detail,
      detailFrame: CGRect(x: textX + dotSize + dotGap, y: metaTop, width: detail?.size.width ?? 0, height: metaHeight),
      due: due, dueFrame: CGRect(x: textRight - dueWidth, y: metaTop, width: dueWidth, height: metaHeight),
      dotFrame: detail != nil && dotSize > 0
        ? CGRect(x: textX, y: metaTop + (metaHeight - dotSize) / 2, width: dotSize, height: dotSize) : .zero)
  }

  private static let accentWidth: CGFloat = 4
  static let dotSize: CGFloat = 8
}
