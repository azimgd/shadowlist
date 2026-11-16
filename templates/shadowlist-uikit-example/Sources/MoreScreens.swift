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
    navigationItem.rightBarButtonItems = []
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
    list.header = ShadowListUtilsListFooter(text: "Touch and hold a sight, then drag it anywhere in the grid.", theme: AppTheme.theme)
    wishlist.tileAspectRatio = 1
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
    navigationItem.rightBarButtonItems = []
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
    list.header = DealsShelf()
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

// MARK: - Rows list

/*
 * A ShadowListKitListView over the library's rows, for the screens that show a kit feature with
 * the kit's own calls. The screen is the list's data: it changes rows and tells the list what
 * changed. With the sl engine sizes come from the rows' layouts, with sl-auto from measuring
 * the cells.
 */
final class RowsList: NSObject, ShadowListKitListViewDataSource {
  let list = ShadowListKitListView()
  var rows: [ShadowListUtilsRow] = []
  private let layouts = ShadowListUtilsLayoutCache()
  private var registered = Set<String>()
  private let sized: Bool

  init(engine: Engine) {
    sized = !engine.selfSizing
    super.init()
    list.backgroundColor = AppTheme.colors.background
    list.contentInsetAdjustmentBehavior = .never
    list.alwaysBounceVertical = true
    list.allowsSelection = false
    list.dataSource = self
  }

  func setRows(_ rows: [ShadowListUtilsRow]) {
    self.rows = rows
    list.reloadData()
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { rows.count }
  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { rows[index].key }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let row = rows[index]
    if registered.insert(row.reuseIdentifier).inserted {
      listView.register(row.viewClass, forCellReuseIdentifier: row.reuseIdentifier)
    }
    let cell = listView.dequeueReusableCell(withIdentifier: row.reuseIdentifier)
    let width = listView.bounds.width / CGFloat(listView.numberOfColumns)
    (cell as? ShadowListUtilsRowView)?.configure(row, layout: sized && width > 0 ? layouts.layout(row, width: width) : nil)
    return cell
  }

  override func responds(to selector: Selector!) -> Bool {
    if selector == #selector(ShadowListKitListViewDataSource.listView(_:sizeForItemAt:crossSize:)) {
      return sized
    }
    return super.responds(to: selector)
  }

  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
    layouts.layout(rows[index], width: crossSize).height
  }
}

/*
 * Travellers as the reorder template's rows, for the screens below.
 */
private func travellerRows(_ count: Int, first: Int = 0) -> [ShadowListUtilsRow] {
  let labels = ShadowListUtilsReorderLabels()
  return (first..<first + count).map { ShadowListUtilsReorderRow(item: Items.contact(Contact(index: $0)), theme: AppTheme.theme, labels: labels) }
}

/*
 * A screen whose content starts under the navigation bar and runs to the bottom of the screen,
 * the frame a React Native screen gives its list.
 */
class PageScreen: UIViewController {
  let page = UIView()

  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = AppTheme.colors.background
    navigationItem.largeTitleDisplayMode = .never
    navigationItem.backButtonDisplayMode = .minimal
    view.addSubview(page)
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    page.frame = CGRect(x: 0, y: view.safeAreaInsets.top, width: view.bounds.width, height: view.bounds.height - view.safeAreaInsets.top)
  }

  /*
   * The More button of the navigation bar, with its actions in inline groups.
   */
  func headerMenu(_ groups: [[UIAction]]) {
    let sections = groups.map { UIMenu(options: .displayInline, children: $0) }
    navigationItem.rightBarButtonItem = UIBarButtonItem(image: UIImage(systemName: "ellipsis"), menu: UIMenu(children: sections))
    navigationItem.rightBarButtonItem?.accessibilityLabel = "More"
  }
}

// MARK: - Horizontal

/*
 * A horizontal strip of destination cards grouped by first letter. The letter of the section
 * in view stays pinned at the start. Card widths follow their titles, and the list starts from
 * an estimate.
 */
final class HorizontalScreen: PageScreen, ShadowListKitListViewDataSource {
  private let engine = Settings.engine
  private let list = ShadowListKitListView()
  private var items: [StripItem] = []
  private let caption = ShadowListUtilsListFooter(text: "Destinations by letter, scroll sideways.", theme: AppTheme.theme)

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Horizontal"
    list.backgroundColor = AppTheme.colors.background
    list.contentInsetAdjustmentBehavior = .never
    list.isHorizontal = true
    list.showsHorizontalScrollIndicator = false
    list.allowsSelection = false
    list.estimatedItemSize = 150
    list.register(StripCell.self, forCellReuseIdentifier: "strip")
    page.addSubview(caption)
    page.addSubview(list)
    load()
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    let captionHeight = caption.sizeThatFits(CGSize(width: page.bounds.width, height: .greatestFiniteMagnitude)).height
    caption.frame = CGRect(x: 0, y: 0, width: page.bounds.width, height: captionHeight)
    list.frame = CGRect(x: 0, y: captionHeight, width: page.bounds.width, height: StripItem.height)
  }

  private func load() {
    let titles = (0..<600).map { (title: FixtureStrings.imageTitles[$0 % FixtureStrings.imageTitles.count], index: $0) }.sorted {
      let order = $0.title.localizedCompare($1.title)
      return order == .orderedSame ? $0.index < $1.index : order == .orderedAscending
    }
    var sticky = IndexSet()
    let palette = AppTheme.colors.avatarPalette
    var letter = ""
    for (title, index) in titles {
      let first = title.prefix(1).uppercased()
      if first != letter {
        letter = first
        sticky.insert(items.count)
        items.append(StripItem(key: "head-\(first)", title: first, header: true, color: .clear))
      }
      items.append(StripItem(key: "strip-\(index)", title: title, header: false, color: palette[items.count % palette.count]))
    }
    list.dataSource = self
    list.stickyIndices = sticky
    list.reloadData()
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { items.count }
  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { items[index].key }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "strip")
    (cell as? StripCell)?.configure(items[index])
    return cell
  }

  /*
   * Widths from the titles, without measuring a cell. The self-sizing engines measure the cell.
   */
  override func responds(to selector: Selector!) -> Bool {
    if selector == #selector(ShadowListKitListViewDataSource.listView(_:sizeForItemAt:crossSize:)) {
      return !engine.selfSizing
    }
    return super.responds(to: selector)
  }

  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
    items[index].width()
  }
}

// MARK: - Changes

/*
 * Travellers that come and go with animatesChanges on. The More menu removes the third row and
 * adds two at the fifth.
 */
final class ChangesScreen: PageScreen {
  private let rows = RowsList(engine: Settings.engine)
  private var fresh = 1000

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Changes"
    page.addSubview(rows.list)
    rows.list.animatesChanges = true
    rows.setRows(travellerRows(80))
    headerMenu([[
      UIAction(title: "Change Travellers", image: UIImage(systemName: "arrow.triangle.2.circlepath")) { [weak self] _ in self?.change() },
    ]])
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    rows.list.frame = page.bounds
  }

  /*
   * Remove the row at 2 and insert two rows at 4, through deleteItems and insertItems.
   */
  func change() {
    guard rows.rows.count > 4 else { return }
    rows.rows.remove(at: 2)
    rows.list.deleteItems(at: IndexSet(integer: 2))
    rows.rows.insert(contentsOf: travellerRows(2, first: fresh), at: 4)
    fresh += 2
    rows.list.insertItems(at: IndexSet(integersIn: 4...5))
  }
}

// MARK: - Collapsing

/*
 * Travellers under a bar that collapses as the list scrolls: an elevated space that scrolls
 * away above a caption that stays pinned. The caption is the list's first item.
 */
final class CollapsingScreen: PageScreen, ShadowListKitListViewDataSource {
  private let list = ShadowListKitListView()
  private let rows = travellerRows(80)
  private var registered = Set<String>()

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Collapsing"
    list.backgroundColor = AppTheme.colors.background
    list.contentInsetAdjustmentBehavior = .never
    list.allowsSelection = false
    list.estimatedItemSize = 64
    let space = UIView(frame: CGRect(x: 0, y: 0, width: 0, height: Self.spaceHeight))
    space.backgroundColor = AppTheme.colors.elevated
    list.headerView = space
    list.register(ListFooterCell.self, forCellReuseIdentifier: "bar")
    list.stickyIndices = IndexSet(integer: 0)
    list.dataSource = self
    page.addSubview(list)
    list.reloadData()
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    list.frame = page.bounds
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { rows.count + 1 }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String {
    index == 0 ? "bar" : rows[index - 1].key
  }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    guard index > 0 else {
      let cell = listView.dequeueReusableCell(withIdentifier: "bar")
      (cell as? ListFooterCell)?.configure("A bar that collapses as the list scrolls.")
      return cell
    }
    let row = rows[index - 1]
    if registered.insert(row.reuseIdentifier).inserted {
      listView.register(row.viewClass, forCellReuseIdentifier: row.reuseIdentifier)
    }
    let cell = listView.dequeueReusableCell(withIdentifier: row.reuseIdentifier)
    (cell as? ShadowListUtilsRowView)?.configure(row, layout: nil)
    return cell
  }

  /*
   * With the caption this makes the 200 point bar of the Android screen.
   */
  private static let spaceHeight: CGFloat = 142
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
