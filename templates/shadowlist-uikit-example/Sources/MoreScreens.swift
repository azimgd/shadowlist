import ShadowListKit
import ShadowListUtils
import UIKit

// MARK: - Reorder

final class ReorderScreen: ListScreen {
  private let boarding = ShadowListUtilsReorderList(theme: AppTheme.theme)
  private var contacts: [ShadowListUtilsContactItem] = (0..<80).map { Items.contact(Contact(index: $0)) }

  override func makeTemplate() -> ShadowListUtilsList? { boarding }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Boarding Order"
    list.header = ShadowListUtilsListFooter(text: "Touch and hold a traveller, then drag to change the order.", theme: AppTheme.theme)
    boarding.onMoveItem = { [weak self] _, _, items in
      self?.contacts = items
      UIImpactFeedbackGenerator(style: .medium).impactOccurred()
    }
    list.onMove = { [weak self] from, to in
      guard let self else { return }
      self.contacts.insert(self.contacts.remove(at: from), at: to)
      UIImpactFeedbackGenerator(style: .medium).impactOccurred()
    }
    list.reorderEnabled = true
    show(
      contacts, .reset, template: { boarding.setItems($0, animated: false) },
      row: { ShadowListUtilsReorderRow(item: $0, theme: AppTheme.theme, labels: ShadowListUtilsReorderLabels()) })
    AutoDrag.run(list: list.view) { [weak self] in self?.contacts.map(\.name) ?? [] }
  }
}

/*
 * Numbered tiles in three columns, dragged into a new order.
 */
final class ReorderGridScreen: ListScreen {
  private let wishlist = ShadowListUtilsReorderGrid(theme: AppTheme.theme)
  private var tiles: [ShadowListUtilsReorderTileItem] = (1...60).map { Items.tile($0) }

  override var columns: Int { 3 }

  override func makeTemplate() -> ShadowListUtilsList? { wishlist }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Sky Wishlist"
    let featuredKey = tiles.first?.key
    wishlist.getItemSpan = { [weak self] index in
      guard let self, self.wishlist.items.indices.contains(index) else { return 1 }
      return self.wishlist.items[index].key == featuredKey ? self.wishlist.numberOfColumns : 1
    }
    wishlist.onMoveItem = { [weak self] _, _, items in
      self?.tiles = items
      UIImpactFeedbackGenerator(style: .medium).impactOccurred()
    }
    list.onMove = { [weak self] from, to in
      guard let self else { return }
      self.tiles.insert(self.tiles.remove(at: from), at: to)
    }
    list.reorderEnabled = true
    show(
      tiles, .reset, template: { wishlist.setItems($0, animated: false) },
      row: { ShadowListUtilsReorderTile(item: $0, aspectRatio: nil, theme: AppTheme.theme, labels: ShadowListUtilsReorderLabels()) })
    AutoDrag.run(list: list.view) { [weak self] in self?.tiles.map(\.title) ?? [] }
  }
}

// MARK: - Snap

final class SnapScreen: ListScreen {
  private let destinations = ShadowListUtilsSnapList(theme: AppTheme.theme)

  override func makeTemplate() -> ShadowListUtilsList? { destinations }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Destinations"
    list.snapToItem = true
    show(
      (0..<50).map { Items.snap($0) }, .reset, template: { destinations.setItems($0, animated: false) },
      row: { ShadowListUtilsSnapCard(item: $0, height: nil, theme: AppTheme.theme, labels: ShadowListUtilsSnapLabels()) })
  }
}

// MARK: - Nested

/*
 * Shelves of pictures: each row holds a horizontal list of cards. The library has no list for
 * these rows, and they go into its base list.
 */
final class NestedScreen: ListScreen {
  private let shelves = ShadowListUtilsList(theme: AppTheme.theme)
  private var generated = 0
  private var items: [ShadowListUtilsNestedItem] = []

  override var prependTitle: String { "Add Shelves at Start" }
  override var appendTitle: String { "Add Shelves at End" }

  override func makeTemplate() -> ShadowListUtilsList? { shelves }

  private func generate(_ count: Int) -> [ShadowListUtilsNestedItem] {
    defer { generated += count }
    return (generated..<generated + count).map { Items.shelf($0) }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Explore"
    list.footer = ShadowListUtilsListFooter(text: "No more destinations", theme: AppTheme.theme)
    items = generate(30)
    show(.reset)
  }

  private func show(_ change: RowChange) {
    show(
      items, change, template: { items in shelves.setRows(items.map(row)) },
      row: row)
  }

  private func row(_ item: ShadowListUtilsNestedItem) -> ShadowListUtilsRow {
    ShadowListUtilsNestedRow(item: item, theme: AppTheme.theme) { [weak self] card, shelf in
      let alert = UIAlertController(title: card.title, message: shelf.title, preferredStyle: .alert)
      alert.addAction(UIAlertAction(title: "OK", style: .default))
      self?.present(alert, animated: true)
    }
  }

  override func prependRows() {
    items = generate(5) + items
    show(.prepend(5))
  }

  override func appendRows() {
    items += generate(5)
    show(.append(5))
  }
}

/*
 * -SLAutoDrag 1 on a reorder screen: pick up the second item, carry it down four rows and drop
 * it, then log the order. Without touches, through the list's testing hooks.
 */
enum AutoDrag {
  static func run(list: UIScrollView, names: @escaping () -> [String]) {
    guard UserDefaults.standard.string(forKey: "SLAutoDrag") == "1", let list = list as? ShadowListKitListView else { return }
    DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
      let before = names().prefix(6)
      let start = list.rectForItem(at: 1)
      var point = CGPoint(x: start.midX, y: start.midY)
      list.shadowListKit_beginDrag(at: point)
      var step = 0
      Timer.scheduledTimer(withTimeInterval: 1.0 / 60, repeats: true) { timer in
        step += 1
        point.y += start.height * 4.2 / 30
        list.shadowListKit_moveDrag(to: point)
        if step == 30 {
          timer.invalidate()
          list.shadowListKit_endDrag()
          DispatchQueue.main.asyncAfter(deadline: .now() + 0.6) {
            print("[SLAUTODRAG] before=\(Array(before)) after=\(Array(names().prefix(6)))")
            fflush(stdout)
          }
        }
      }
    }
  }
}
