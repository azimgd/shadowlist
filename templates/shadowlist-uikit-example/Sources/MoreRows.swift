import ShadowListKit
import ShadowListUtils
import UIKit

// MARK: - Horizontal

/*
 * An item of the horizontal list: a section's letter, pinned while its section scrolls by,
 * or a card as wide as its title.
 */
struct StripItem {
  let key: String
  let title: String
  let header: Bool
  let color: UIColor

  /*
   * The typeset title, one line that truncates at the widest card.
   */
  var titleLayout: ShadowListKitTextLayout {
    Self.titleStyle.layout(title, width: Self.maxWidth - 2 * Self.paddingHorizontal, maxLines: 1)
  }

  /*
   * The width of the item, from its title. Safe on any thread.
   */
  func width() -> CGFloat {
    if header { return Self.headerWidth }
    return min(Self.maxWidth, max(Self.minWidth, titleLayout.size.width + 2 * Self.paddingHorizontal))
  }

  static let titleStyle = AppTheme.typography.subhead.with(.semibold)
  static let height: CGFloat = 140
  private static let headerWidth: CGFloat = 44
  private static let minWidth: CGFloat = 96
  private static let maxWidth: CGFloat = 260
  private static let paddingHorizontal: CGFloat = 16
}

/*
 * A cell that knows its own width for an exact height, for a horizontal list that measures
 * its cells.
 */
final class StripCell: ShadowListKitListCell {
  private let card = UIView()
  private let title = ShadowListKitTextView(frame: .zero)
  private var item: StripItem?

  override init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    card.layer.cornerRadius = AppTheme.theme.radius.md
    addSubview(card)
    addSubview(title)
  }

  var key: String? { item?.key }

  func configure(_ item: StripItem) {
    self.item = item
    card.backgroundColor = item.header ? AppTheme.colors.elevated2 : item.color
    title.textColor = item.header ? AppTheme.colors.secondaryLabel : AppTheme.colors.onAccent
    title.textLayout = item.titleLayout
    setNeedsLayout()
  }

  override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: item?.width() ?? 0, height: size.height)
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    card.frame = bounds.insetBy(dx: Self.cardInsetHorizontal, dy: Self.cardInsetVertical)
    guard let text = title.textLayout else { return }
    let scale = UIScreen.main.scale
    title.frame = CGRect(
      x: ((bounds.width - text.size.width) / 2 * scale).rounded() / scale, y: ((bounds.height - text.size.height) / 2 * scale).rounded() / scale,
      width: text.size.width, height: text.size.height)
  }

  private static let cardInsetHorizontal: CGFloat = 4
  private static let cardInsetVertical: CGFloat = 8
}

// MARK: - List footer

/*
 * The library's ListFooter as a cell: a section's footer, or an item that is a caption. The
 * list sizes it through sizeThatFits.
 */
final class ListFooterCell: ShadowListKitListCell {
  private var footer: ShadowListUtilsListFooter?

  func configure(_ text: String) {
    guard footer?.text != text else { return }
    footer?.removeFromSuperview()
    let footer = ShadowListUtilsListFooter(text: text, theme: AppTheme.theme)
    addSubview(footer)
    self.footer = footer
    setNeedsLayout()
  }

  override func sizeThatFits(_ size: CGSize) -> CGSize {
    footer?.sizeThatFits(size) ?? CGSize(width: size.width, height: 0)
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    footer?.frame = bounds
  }
}
