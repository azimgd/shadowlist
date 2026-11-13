import UIKit

/*
 * A grid of tiles in three columns the reader drags into a new order: touch and hold a tile,
 * then drag it.
 */
public final class ShadowListUtilsReorderGrid: ShadowListUtilsList {
  /*
   * The tiles. Setting them shows them without an animation.
   */
  public var items: [ShadowListUtilsReorderTileItem] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  /*
   * Show these items. animated animates the change from the items before.
   */
  public func setItems(_ items: [ShadowListUtilsReorderTileItem], animated: Bool) {
    let rows = rows(for: items, previousItems: storedItems, previousRows: rows, key: \.key, make: makeRow)
    storedItems = items
    setRows(rows, animated: animated)
  }

  /*
   * Runs for a tapped tile. Without it the tiles do not select.
   */
  public var onPressItem: ((ShadowListUtilsReorderTileItem) -> Void)? {
    didSet { allowsSelection = onPressItem != nil }
  }

  /*
   * Replaces the tile with a row of the app's own for every item.
   */
  public var rowForItem: ((ShadowListUtilsReorderTileItem) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsReorderLabels() {
    didSet { reloadRows() }
  }

  /*
   * Runs after a tile was dropped at a new place, or moved by one place through a screen
   * reader action, with the source index, the destination index and the items after the move.
   * items already holds that order. A tile dropped where it was picked up moved nowhere and
   * does not run it.
   */
  public var onMoveItem: ((Int, Int, [ShadowListUtilsReorderTileItem]) -> Void)?

  /*
   * Gives every swatch the same width over height, in place of each item's own.
   */
  public var tileAspectRatio: CGFloat? {
    didSet {
      if tileAspectRatio != oldValue {
        rebuildRows()
      }
    }
  }

  private var storedItems: [ShadowListUtilsReorderTileItem] = []

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    reorderEnabled = true
    numberOfColumns = Self.columns
    allowsSelection = false
  }

  private func makeRow(_ item: ShadowListUtilsReorderTileItem) -> ShadowListUtilsRow {
    rowForItem?(item) ?? ShadowListUtilsReorderTile(item: item, aspectRatio: tileAspectRatio, theme: theme, labels: labels)
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

  /*
   * The kit dropped a tile. It reads the data again right after, which is already in the new
   * order here.
   */
  override func moveItem(from sourceIndex: Int, to destinationIndex: Int) {
    guard sourceIndex != destinationIndex else { return }
    storedItems = ShadowListUtilsArrayMove.moved(storedItems, from: sourceIndex, to: destinationIndex)
    super.moveItem(from: sourceIndex, to: destinationIndex)
    onMoveItem?(sourceIndex, destinationIndex, storedItems)
  }

  override func accessibilityActions(at index: Int) -> [UIAccessibilityCustomAction] {
    guard onMoveItem != nil else { return [] }
    return [
      UIAccessibilityCustomAction(name: labels.moveEarlier) { [weak self] _ in self?.moveItem(at: index, by: -1) ?? false },
      UIAccessibilityCustomAction(name: labels.moveLater) { [weak self] _ in self?.moveItem(at: index, by: 1) ?? false },
    ]
  }

  /*
   * Move an item by one place for a screen reader, the same path as a drop.
   */
  private func moveItem(at index: Int, by offset: Int) -> Bool {
    let destinationIndex = index + offset
    guard storedItems.indices.contains(index), storedItems.indices.contains(destinationIndex) else { return false }
    setItems(ShadowListUtilsArrayMove.moved(storedItems, from: index, to: destinationIndex), animated: true)
    onMoveItem?(index, destinationIndex, storedItems)
    return true
  }

  private static let columns = 3
}
