import ShadowListKit
import UIKit

/*
 * A card of a snap list, height tall with its margin: a rounded card filled by an image, with a
 * caption of a title and a subtitle over its bottom edge. A snap list gives every card its
 * height. A nil height means a quarter of the window's height.
 */
public struct ShadowListUtilsSnapCard: ShadowListUtilsRow {
  public let item: ShadowListUtilsSnapItem
  public let height: CGFloat?
  public let theme: ShadowListUtilsTheme
  public let labels: ShadowListUtilsSnapLabels
  public let contentVersion: Int

  public init(
    item: ShadowListUtilsSnapItem, height: CGFloat?, theme: ShadowListUtilsTheme, labels: ShadowListUtilsSnapLabels
  ) {
    self.item = item
    self.height = height
    self.theme = theme
    self.labels = labels
    var hasher = Hasher()
    hasher.combine(item)
    hasher.combine(height)
    contentVersion = hasher.finalize()
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsSnapCardView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let spacing = theme.spacing
    let direction = ShadowListUtilsDirection(width: width)
    let height = self.height ?? Self.defaultHeight(windowHeight: ShadowListUtilsPlatform.windowHeight)
    let cardFrame = CGRect(x: spacing.sm, y: spacing.sm, width: max(0, width - 2 * spacing.sm), height: max(0, height - 2 * spacing.sm))
    let textWidth = cardFrame.width - 2 * spacing.lg
    let title = item.title.map { theme.typography.title3.layout($0, width: textWidth, maxLines: Self.textLines) }
    let subtitle = item.subtitle.map { theme.typography.subhead.layout($0, width: textWidth, maxLines: Self.textLines) }

    // Frames inside the card. The caption sits on the card's bottom edge.
    var captionFrame = CGRect.zero
    var titleFrame = CGRect.zero
    var subtitleFrame = CGRect.zero
    if title != nil || subtitle != nil {
      let titleHeight = title?.size.height ?? 0
      let subtitleHeight = subtitle?.size.height ?? 0
      let captionHeight = titleHeight + subtitleHeight + 2 * spacing.lg
      captionFrame = CGRect(x: 0, y: cardFrame.height - captionHeight, width: cardFrame.width, height: captionHeight)
      titleFrame = title.map { CGRect(CGPoint(x: spacing.lg, y: spacing.lg), $0) } ?? .zero
      subtitleFrame = subtitle.map { CGRect(CGPoint(x: spacing.lg, y: spacing.lg + titleHeight), $0) } ?? .zero
    }
    return ShadowListUtilsSnapCardLayout(
      width: width, height: height, cardFrame: direction.place(cardFrame), captionFrame: captionFrame, title: title,
      titleFrame: direction.place(titleFrame, in: cardFrame.width), subtitle: subtitle,
      subtitleFrame: direction.place(subtitleFrame, in: cardFrame.width))
  }

  /*
   * The height of a card without one of its own: a quarter of the window's height, on the pixel
   * grid.
   */
  static func defaultHeight(windowHeight: CGFloat) -> CGFloat {
    ShadowListUtilsPlatform.roundToPixel(windowHeight * windowShare)
  }

  private static let textLines = 2
  private static let windowShare: CGFloat = 0.25
}

/*
 * cardFrame is in the row. captionFrame is in the card, and the text frames are in the caption.
 */
public final class ShadowListUtilsSnapCardLayout: ShadowListUtilsRowLayout {
  public let cardFrame: CGRect
  public let captionFrame: CGRect
  public let title: ShadowListKitTextLayout?
  public let titleFrame: CGRect
  public let subtitle: ShadowListKitTextLayout?
  public let subtitleFrame: CGRect

  init(
    width: CGFloat, height: CGFloat, cardFrame: CGRect, captionFrame: CGRect, title: ShadowListKitTextLayout?, titleFrame: CGRect,
    subtitle: ShadowListKitTextLayout?, subtitleFrame: CGRect
  ) {
    self.cardFrame = cardFrame
    self.captionFrame = captionFrame
    self.title = title
    self.titleFrame = titleFrame
    self.subtitle = subtitle
    self.subtitleFrame = subtitleFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsSnapCardView: ShadowListUtilsRowView {
  private let card = UIView()
  private let image = ShadowListUtilsImageView()
  private let caption = UIView()
  private let title = ShadowListKitTextView(frame: .zero)
  private let subtitle = ShadowListKitTextView(frame: .zero)

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    card.clipsToBounds = true
    image.placeholderColor = .clear
    caption.backgroundColor = Self.captionColor
    caption.addSubview(title)
    caption.addSubview(subtitle)
    card.addSubview(image)
    card.addSubview(caption)
    addSubview(card)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsSnapCard else { return }
    let item = row.item
    restingBackgroundColor = row.theme.colors.background
    card.backgroundColor = item.color ?? row.theme.colors.elevated
    card.layer.cornerRadius = row.theme.radius.lg
    image.isHidden = item.imageUrl == nil
    image.url = item.imageUrl
    let text = [item.title, item.subtitle].compactMap { $0 }.filter { !$0.isEmpty }.joined(separator: ", ")
    accessibilityLabel = text.isEmpty ? item.imageAlt ?? row.labels.card : text
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let layout = layout as? ShadowListUtilsSnapCardLayout else { return }
    card.frame = layout.cardFrame
    image.frame = card.bounds
    caption.isHidden = layout.title == nil && layout.subtitle == nil
    caption.frame = layout.captionFrame
    title.isHidden = layout.title == nil
    title.show(layout.title, .white)
    title.frame = layout.titleFrame
    subtitle.isHidden = layout.subtitle == nil
    subtitle.show(layout.subtitle, Self.subtitleColor)
    subtitle.frame = layout.subtitleFrame
  }

  private static let captionColor = UIColor(white: 0, alpha: 0.35)
  private static let subtitleColor = UIColor(white: 1, alpha: 0.8)
}
