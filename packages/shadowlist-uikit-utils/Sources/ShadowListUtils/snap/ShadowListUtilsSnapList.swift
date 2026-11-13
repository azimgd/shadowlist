import UIKit

/*
 * A list of tall cards whose scroll position rests on a card's leading edge.
 */
public final class ShadowListUtilsSnapList: ShadowListUtilsList {
  /*
   * The cards. Setting them shows them without an animation.
   */
  public var items: [ShadowListUtilsSnapItem] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  /*
   * Show these items. animated animates the change from the items before.
   */
  public func setItems(_ items: [ShadowListUtilsSnapItem], animated: Bool) {
    let rows = rows(for: items, previousItems: storedItems, previousRows: rows, key: \.key, make: makeRow)
    storedItems = items
    setRows(rows, animated: animated)
  }

  /*
   * Runs for a tapped card. Without it the cards do not select.
   */
  public var onPressItem: ((ShadowListUtilsSnapItem) -> Void)? {
    didSet { allowsSelection = onPressItem != nil }
  }

  /*
   * Replaces the card with a row of the app's own for every item.
   */
  public var rowForItem: ((ShadowListUtilsSnapItem) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsSnapLabels() {
    didSet { reloadRows() }
  }

  /*
   * The height of a card with its margin. nil means a quarter of the height of the window the
   * list is in.
   */
  public var cardHeight: CGFloat? {
    didSet {
      if cardHeight != oldValue {
        rebuildRows()
      }
    }
  }

  private var storedItems: [ShadowListUtilsSnapItem] = []

  /*
   * The height of the window the cards without a height were last sized for.
   */
  private var windowHeight = ShadowListUtilsPlatform.windowHeight

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    snapToItem = true
    allowsSelection = false
  }

  /*
   * A window of another height gives the cards without a height of their own a new one.
   */
  public override func layoutSubviews() {
    if let window, window.bounds.height != windowHeight {
      windowHeight = window.bounds.height
      ShadowListUtilsPlatform.resolve(window: window)
      if cardHeight == nil {
        rebuildRows()
      }
    }
    super.layoutSubviews()
  }

  private func makeRow(_ item: ShadowListUtilsSnapItem) -> ShadowListUtilsRow {
    let height = cardHeight ?? ShadowListUtilsSnapCard.defaultHeight(windowHeight: windowHeight)
    return rowForItem?(item) ?? ShadowListUtilsSnapCard(item: item, height: height, theme: theme, labels: labels)
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
}
