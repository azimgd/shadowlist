import UIKit

/*
 * A feed of posts.
 */
public final class ShadowListUtilsFeedList: ShadowListUtilsList {
  /*
   * The posts. Setting them shows them without an animation.
   */
  public var items: [ShadowListUtilsFeedItem] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  /*
   * Show these items. animated animates the change from the items before.
   */
  public func setItems(_ items: [ShadowListUtilsFeedItem], animated: Bool) {
    let rows = rows(for: items, previousItems: storedItems, previousRows: rows, key: \.key, make: makeRow)
    storedItems = items
    setRows(rows, animated: animated)
  }

  /*
   * Runs for a tapped post. Without it the posts do not select.
   */
  public var onPressItem: ((ShadowListUtilsFeedItem) -> Void)? {
    didSet { allowsSelection = onPressItem != nil }
  }

  /*
   * Replaces the post row with a row of the app's own for every item.
   */
  public var rowForItem: ((ShadowListUtilsFeedItem) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsFeedLabels() {
    didSet { reloadRows() }
  }

  /*
   * Runs for a tapped image with its post and its position among the post's images. While it
   * is set a screen reader reads every image as a button.
   */
  public var onPressImage: ((ShadowListUtilsFeedItem, Int) -> Void)?

  /*
   * Writes a post's age from its createdAtMs in place of the labels' words.
   */
  public var formatTime: ((Double) -> String)? {
    didSet { reloadRows() }
  }

  private var storedItems: [ShadowListUtilsFeedItem] = []

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    allowsSelection = false
  }

  private func makeRow(_ item: ShadowListUtilsFeedItem) -> ShadowListUtilsRow {
    rowForItem?(item) ?? ShadowListUtilsFeedRow(item: item, theme: theme, labels: labels, formatTime: formatTime)
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
