import UIKit

/*
 * Activities in the app's order. Presses are controlled by the app; the list never marks an
 * activity read itself. Pull to refresh and edge callbacks come from ShadowListUtilsList.
 */
public final class ShadowListUtilsActivityList: ShadowListUtilsList {
  public var items: [ShadowListUtilsActivityItem] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  public func setItems(_ items: [ShadowListUtilsActivityItem], animated: Bool) {
    let rows = rows(for: items, previousItems: storedItems, previousRows: rows, key: \.key, make: makeRow)
    storedItems = items
    setRows(rows, animated: animated)
  }

  public var onPressItem: ((ShadowListUtilsActivityItem) -> Void)? {
    didSet {
      allowsSelection = onPressItem != nil
      rebuildRows()
    }
  }

  public var rowForItem: ((ShadowListUtilsActivityItem) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsActivityLabels() {
    didSet { reloadRows() }
  }

  public var formatTime: ((Double) -> String)? {
    didSet { reloadRows() }
  }

  private var storedItems: [ShadowListUtilsActivityItem] = []

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    allowsSelection = false
    showsSeparators = true
  }

  private func makeRow(_ item: ShadowListUtilsActivityItem) -> ShadowListUtilsRow {
    rowForItem?(item) ?? ShadowListUtilsActivityRow(
      item: item, interactive: onPressItem != nil, theme: theme, labels: labels, formatTime: formatTime)
  }

  override func makeRows() -> [ShadowListUtilsRow]? { storedItems.map(makeRow) }

  override func didSelectItem(at index: Int) {
    if storedItems.indices.contains(index) { onPressItem?(storedItems[index]) }
    deselectItem(at: index, animated: true)
  }
}
