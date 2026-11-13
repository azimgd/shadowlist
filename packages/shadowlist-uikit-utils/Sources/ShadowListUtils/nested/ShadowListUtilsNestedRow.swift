import ShadowListKit
import UIKit

/*
 * A titled row that holds a horizontal list of cards. onPressCard runs for a tapped card with
 * the card and the row's item. Put these rows into a ShadowListUtilsList with setRows. The
 * horizontal list keeps its scroll position while its row is off screen.
 */
public struct ShadowListUtilsNestedRow: ShadowListUtilsRow {
  public let item: ShadowListUtilsNestedItem
  public let theme: ShadowListUtilsTheme
  public let onPressCard: ((ShadowListUtilsNestedCardItem, ShadowListUtilsNestedItem) -> Void)?
  public let contentVersion: Int

  public init(
    item: ShadowListUtilsNestedItem,
    theme: ShadowListUtilsTheme,
    onPressCard: ((ShadowListUtilsNestedCardItem, ShadowListUtilsNestedItem) -> Void)? = nil
  ) {
    self.item = item
    self.theme = theme
    self.onPressCard = onPressCard
    contentVersion = item.hashValue
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsNestedRowView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let spacing = theme.spacing
    let direction = ShadowListUtilsDirection(width: width)
    let title = theme.typography.title3.layout(item.title, width: width - 2 * spacing.lg, maxLines: 1)
    let titleFrame = CGRect(CGPoint(x: spacing.lg, y: 0), title)
    let listTop = title.size.height + spacing.md
    // The list is as tall as a card, and the row adds the space under it.
    let listFrame = CGRect(x: 0, y: listTop, width: width, height: ShadowListUtilsNestedCard.height(theme))
    return ShadowListUtilsNestedRowLayout(
      width: width, height: listFrame.maxY + spacing.lg, title: title, titleFrame: direction.place(titleFrame), listFrame: listFrame)
  }
}

public final class ShadowListUtilsNestedRowLayout: ShadowListUtilsRowLayout {
  public let title: ShadowListKitTextLayout
  public let titleFrame: CGRect
  public let listFrame: CGRect

  init(width: CGFloat, height: CGFloat, title: ShadowListKitTextLayout, titleFrame: CGRect, listFrame: CGRect) {
    self.title = title
    self.titleFrame = titleFrame
    self.listFrame = listFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsNestedRowView: ShadowListUtilsRowView {
  private let title = ShadowListKitTextView(frame: .zero)
  private let cards = ShadowListUtilsNestedCardList()

  /*
   * The key of the row the cards show now, and the key of the row whose saved place they have
   * yet to land on.
   */
  private var shownKey: String?
  private var restoringKey: String?

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    title.isAccessibilityElement = true
    title.accessibilityTraits = .header
    addSubview(title)
    addSubview(cards)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsNestedRow else { return }
    restingBackgroundColor = row.theme.colors.background
    title.accessibilityLabel = row.item.title
    // A row that comes on screen lands where it was left. A row shown already keeps its place.
    if shownKey != row.key {
      shownKey = row.key
      restoringKey = row.key
    }
    cards.show(row)
  }

  override func replaceRow(_ row: ShadowListUtilsRow) {
    super.replaceRow(row)
    if let row = row as? ShadowListUtilsNestedRow {
      cards.replaceRow(row)
    }
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsNestedRow, let layout = layout as? ShadowListUtilsNestedRowLayout else { return }
    title.show(layout.title, row.theme.colors.label)
    title.frame = layout.titleFrame
    cards.frame = layout.listFrame
    restorePosition()
  }

  /*
   * The cards land where the reader left them, or at the start for a row seen the first time.
   * It waits for the cell to be in the outer list, which holds the saved places.
   */
  private func restorePosition() {
    guard let key = restoringKey, let outer = enclosingList else { return }
    restoringKey = nil
    if let state = (outer as? ShadowListUtilsList)?.nestedAnchors[key] {
      cards.restoreAnchorState(state)
    } else {
      cards.scrollToStart(animated: false)
    }
  }

  /*
   * Keep where the cards were, by the row's key, in the list that owns the row.
   */
  override func didEndDisplaying(in list: ShadowListUtilsList) {
    guard let key = row?.key ?? shownKey else { return }
    list.nestedAnchors[key] = cards.anchorState
    shownKey = nil
  }
}

/*
 * The horizontal list of a nested row. A tap on a card goes to the row's onPressCard.
 */
final class ShadowListUtilsNestedCardList: ShadowListUtilsList {
  private var nested: ShadowListUtilsNestedRow?

  init() {
    super.init(theme: .system)
    isHorizontal = true
    showsHorizontalScrollIndicator = false
    scrollsToTop = false
    allowsSelection = false
  }

  func show(_ row: ShadowListUtilsNestedRow) {
    theme = row.theme
    replaceRow(row)
    setRows(row.item.cards.map { ShadowListUtilsNestedCard(item: $0, theme: row.theme) })
  }

  /*
   * Take a row with the same cards, like one built again with a new onPressCard.
   */
  func replaceRow(_ row: ShadowListUtilsNestedRow) {
    nested = row
    allowsSelection = row.onPressCard != nil
  }

  override func didSelectItem(at index: Int) {
    if let nested, nested.item.cards.indices.contains(index) {
      nested.onPressCard?(nested.item.cards[index], nested.item)
    }
    deselectItem(at: index, animated: true)
  }
}
