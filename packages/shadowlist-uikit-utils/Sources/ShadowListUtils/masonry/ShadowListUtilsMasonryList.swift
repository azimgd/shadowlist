import UIKit

/*
 * A grid of image cards in three columns. Each card keeps its image's aspect, and the columns
 * stack their own cards.
 */
public final class ShadowListUtilsMasonryList: ShadowListUtilsList {
  /*
   * The cards. Setting them shows them without an animation.
   */
  public var items: [ShadowListUtilsMasonryItem] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  /*
   * Show these items. animated animates the change from the items before.
   */
  public func setItems(_ items: [ShadowListUtilsMasonryItem], animated: Bool) {
    let rows = rows(for: items, previousItems: storedItems, previousRows: rows, key: \.key, make: makeRow)
    storedItems = items
    setRows(rows, animated: animated)
  }

  /*
   * Runs for a tapped card. Without it the cards do not select.
   */
  public var onPressItem: ((ShadowListUtilsMasonryItem) -> Void)? {
    didSet { allowsSelection = onPressItem != nil }
  }

  /*
   * Replaces the card with a row of the app's own for every item.
   */
  public var rowForItem: ((ShadowListUtilsMasonryItem) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsMasonryLabels() {
    didSet { reloadRows() }
  }

  private var storedItems: [ShadowListUtilsMasonryItem] = []

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    numberOfColumns = Self.columns
    allowsSelection = false
  }

  private func makeRow(_ item: ShadowListUtilsMasonryItem) -> ShadowListUtilsRow {
    rowForItem?(item) ?? ShadowListUtilsMasonryCard(item: item, theme: theme, labels: labels)
  }

  override func makeRows() -> [ShadowListUtilsRow]? {
    storedItems.map(makeRow)
  }

  override func didSelectItem(at index: Int) {
    if storedItems.indices.contains(index) {
      onPressItem?(storedItems[index])
    }
    deselectItem(at: index, animated: true)
  }

  private static let columns = 3
}
