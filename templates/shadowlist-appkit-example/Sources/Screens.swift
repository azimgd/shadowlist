import AppKit
import ShadowListKit

/*
 * A screen with one list the scenarios read and change.
 */
protocol ListScreen: AnyObject {
  var list: ShadowListKitListView { get }
  var keys: [String] { get }
  func prependRows()
  func appendRows()
}

/*
 * Posts with heights from layouts typeset ahead, separators between them. The list scrolls by
 * itself, a post dragged with the mouse moves and a post swiped on the trackpad shows its
 * actions.
 */
final class FeedScreen: NSView, ListScreen, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
  let list = ShadowListKitListView()
  private var posts: [FeedPost]
  private var layouts: [String: FeedLayout] = [:]
  private var nextIndex: Int
  private var previousIndex = -1
  private(set) var activated: [Int] = []
  private(set) var moves: [[Int]] = []
  private(set) var pinned: Set<String> = []
  private(set) var flagged: Set<String> = []
  var showsMenus = true

  var keys: [String] { posts.map(\.key) }

  func name(at index: Int) -> String { posts[index].name }

  init(count: Int) {
    posts = (0..<count).map { FeedPost(index: $0) }
    nextIndex = count
    super.init(frame: .zero)
    list.showsSeparators = true
    list.allowsMultipleSelection = true
    list.reorderEnabled = true
    list.separatorInsetStart = FeedLayout.padding + FeedLayout.avatar + FeedLayout.gap
    list.register(FeedCell.self, forCellReuseIdentifier: "post")
    list.dataSource = self
    list.delegate = self
    list.identifier = NSUserInterfaceItemIdentifier("feed")
    list.autoresizingMask = [.width, .height]
    addSubview(list)
    list.reloadData()
  }

  required init?(coder: NSCoder) { fatalError() }

  override var isFlipped: Bool { true }

  override func viewDidMoveToWindow() {
    super.viewDidMoveToWindow()
    list.frame = bounds
  }

  private func layout(at index: Int, width: CGFloat) -> FeedLayout {
    let post = posts[index]
    if let layout = layouts[post.key], layout.width == width {
      return layout
    }
    let layout = FeedLayout(post: post, width: width)
    layouts[post.key] = layout
    return layout
  }

  /*
   * New posts get new generation indices: older ones count down from -1, newer ones up.
   */
  func prependRows() {
    let added = (0..<10).map { _ -> FeedPost in
      defer { previousIndex -= 1 }
      return FeedPost(index: previousIndex + 1_000_000)
    }
    posts.insert(contentsOf: added.reversed(), at: 0)
    list.insertItems(at: IndexSet(0..<added.count))
  }

  func appendRows() {
    let start = posts.count
    posts += (0..<20).map { _ -> FeedPost in
      defer { nextIndex += 1 }
      return FeedPost(index: nextIndex)
    }
    list.insertItems(at: IndexSet(start..<posts.count))
  }

  /*
   * Delete the post after an index and add a new one after the next, in one reload.
   */
  func replacePost(after index: Int) {
    posts.remove(at: index + 1)
    posts.insert(FeedPost(index: 900_000), at: index + 2)
    list.reloadData()
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { posts.count }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { posts[index].key }

  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
    layout(at: index, width: crossSize).height
  }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "post") as! FeedCell
    cell.configure(posts[index], layout: layout(at: index, width: listView.contentSize.width))
    return cell
  }

  func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, to destinationIndex: Int) {
    moves.append([sourceIndex, destinationIndex])
    posts.insert(posts.remove(at: sourceIndex), at: destinationIndex)
  }

  func listView(_ listView: ShadowListKitListView, didActivateItemAt index: Int) {
    activated.append(index)
  }

  func listView(_ listView: ShadowListKitListView, typeSelectStringForItemAt index: Int) -> String? {
    posts[index].name
  }

  /*
   * A post's leading side pins it, its trailing side deletes or flags it. A full swipe performs
   * the first action of the side.
   */
  func listView(_ listView: ShadowListKitListView, leadingSwipeActionsForItemAt index: Int) -> ShadowListKitSwipeActionsConfiguration? {
    let key = posts[index].key
    let isPinned = pinned.contains(key)
    let pin = ShadowListKitSwipeAction(style: .normal, title: isPinned ? "Unpin" : "Pin") { [weak self] _, done in
      if isPinned {
        self?.pinned.remove(key)
      } else {
        self?.pinned.insert(key)
      }
      done(true)
    }
    pin.image = NSImage(systemSymbolName: isPinned ? "pin.slash.fill" : "pin.fill", accessibilityDescription: nil)
    pin.backgroundColor = .systemBlue
    return ShadowListKitSwipeActionsConfiguration(actions: [pin])
  }

  func listView(_ listView: ShadowListKitListView, trailingSwipeActionsForItemAt index: Int) -> ShadowListKitSwipeActionsConfiguration? {
    let key = posts[index].key
    let delete = ShadowListKitSwipeAction(style: .destructive, title: "Delete") { [weak self] _, done in
      guard let self, let at = self.posts.firstIndex(where: { $0.key == key }) else { return done(false) }
      self.posts.remove(at: at)
      self.list.deleteItems(at: IndexSet(integer: at))
      done(true)
    }
    delete.image = NSImage(systemSymbolName: "trash.fill", accessibilityDescription: nil)
    let flag = ShadowListKitSwipeAction(style: .normal, title: flagged.contains(key) ? "Unflag" : "Flag") { [weak self] _, done in
      guard let self else { return done(false) }
      if self.flagged.remove(key) == nil {
        self.flagged.insert(key)
      }
      done(true)
    }
    flag.image = NSImage(systemSymbolName: "flag.fill", accessibilityDescription: nil)
    flag.backgroundColor = .systemOrange
    return ShadowListKitSwipeActionsConfiguration(actions: [delete, flag])
  }

  /*
   * With showsMenus off a post shows its swipe actions as its menu.
   */
  func listView(_ listView: ShadowListKitListView, contextMenuForItemAt index: Int) -> NSMenu? {
    guard showsMenus else { return nil }
    let menu = NSMenu(title: posts[index].key)
    menu.addItem(withTitle: "Open", action: nil, keyEquivalent: "")
    menu.addItem(withTitle: "Copy Name", action: nil, keyEquivalent: "")
    return menu
  }
}

/*
 * A host that moves a driven list the way a two axis grid does. A two axis scroll engine takes
 * the wheel: down the rows through the list's contentOffset and scrollPhase, sideways by
 * shifting every row's tiles. Overlay scrollers show both axes. Offsets the list asks for land at
 * once, in the engine too. Some rows are taller, as wrapped cells would make them.
 */
final class DrivenScreen: NSView, ListScreen, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate,
  ShadowListKitScrollEngineDelegate, ShadowListKitScrollerViewDelegate
{
  static let columnCount = 40

  let list = ShadowListKitListView()
  let scrollers = ShadowListKitScrollerView()
  private(set) lazy var engine = ShadowListKitScrollEngine(view: self)
  private(set) var rows: [String]
  private(set) var columnOffset: CGFloat = 0
  private(set) var corrections = 0
  private var previousIndex = -1

  /*
   * The cells the list shows, from willDisplay and didEndDisplaying. Asking for visibleCells on
   * every move would bring each view up to the layer the list moved it by.
   */
  private let shownCells = NSHashTable<GridRowCell>.weakObjects()

  var keys: [String] { rows }

  init(count: Int) {
    rows = (0..<count).map { "row-\($0)" }
    super.init(frame: .zero)
    list.isScrollDriven = true
    list.allowsSelection = false
    list.estimatedItemSize = 24
    list.register(GridRowCell.self, forCellReuseIdentifier: "row")
    list.dataSource = self
    list.delegate = self
    list.autoresizingMask = [.width, .height]
    addSubview(list)
    engine.axes = [.horizontal, .vertical]
    engine.delegate = self
    scrollers.axes = [.horizontal, .vertical]
    scrollers.delegate = self
    scrollers.autoresizingMask = [.width, .height]
    addSubview(scrollers)
    list.reloadData()
  }

  required init?(coder: NSCoder) { fatalError() }

  override var isFlipped: Bool { true }

  override func viewDidMoveToWindow() {
    super.viewDidMoveToWindow()
    list.frame = bounds
    scrollers.frame = bounds
  }

  var contentSize: CGSize {
    CGSize(
      width: GridRowCell.numberWidth + CGFloat(DrivenScreen.columnCount) * GridRowCell.columnWidth,
      height: list.contentSize.height)
  }

  var maxColumnOffset: CGFloat {
    max(0, GridRowCell.numberWidth + CGFloat(DrivenScreen.columnCount) * GridRowCell.columnWidth - list.bounds.width)
  }

  var maxRowOffset: CGFloat {
    max(0, list.contentSize.height - list.bounds.height)
  }

  /*
   * Move both axes inside the content, the way the scripted drags do.
   */
  func scroll(to point: CGPoint) {
    move(to: CGPoint(x: min(max(0, point.x), maxColumnOffset), y: min(max(0, point.y), maxRowOffset)))
  }

  /*
   * Put the rows and the tiles at an offset, past the edges while the engine rubber bands.
   */
  private func move(to point: CGPoint) {
    columnOffset = point.x
    list.contentOffset = CGPoint(x: 0, y: point.y)
    for cell in shownCells.allObjects {
      cell.shift = columnOffset
    }
    syncScrolling()
  }

  private func syncScrolling() {
    let offset = CGPoint(x: columnOffset, y: list.contentOffset.y)
    engine.viewportSize = bounds.size
    engine.contentSize = contentSize
    engine.offset = offset
    scrollers.contentSize = contentSize
    scrollers.offset = offset
  }

  override func scrollWheel(with event: NSEvent) {
    if !engine.handleScrollWheel(event) {
      super.scrollWheel(with: event)
    }
  }

  func scrollEngine(_ engine: ShadowListKitScrollEngine, didScrollTo offset: CGPoint) {
    move(to: offset)
  }

  func scrollEngine(_ engine: ShadowListKitScrollEngine, didChange phase: ShadowListKitScrollPhase) {
    list.scrollPhase = phase
  }

  func scrollerView(_ scrollerView: ShadowListKitScrollerView, didScrollTo offset: CGPoint) {
    engine.stopScrolling()
    scroll(to: offset)
  }

  func listViewDidLayOut(_ listView: ShadowListKitListView) {
    syncScrolling()
  }

  func prependRows() {
    let added = (0..<15).map { _ -> String in
      defer { previousIndex -= 1 }
      return "row\(previousIndex)"
    }
    rows.insert(contentsOf: added.reversed(), at: 0)
    list.insertItems(at: IndexSet(0..<added.count))
  }

  func appendRows() {
    let start = rows.count
    rows += (start..<start + 20).map { "row-\($0)" }
    list.insertItems(at: IndexSet(start..<rows.count))
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { rows.count }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { rows[index] }

  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
    rows[index].utf8.reduce(0) { ($0 * 31 + Int($1)) % 65_521 } % 7 == 0 ? 44 : 22
  }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "row") as! GridRowCell
    cell.columnCount = DrivenScreen.columnCount
    cell.configure(row: index)
    cell.shift = columnOffset
    return cell
  }

  func listView(_ listView: ShadowListKitListView, didCorrectOffset contentOffset: CGPoint) {
    corrections += 1
    listView.contentOffset = contentOffset
    engine.offset = CGPoint(x: columnOffset, y: contentOffset.y)
  }

  func listView(_ listView: ShadowListKitListView, willDisplay cell: ShadowListKitListCell, at index: Int) {
    guard let cell = cell as? GridRowCell else { return }
    cell.shift = columnOffset
    shownCells.add(cell)
  }

  func listView(_ listView: ShadowListKitListView, didEndDisplaying cell: ShadowListKitListCell, at index: Int) {
    (cell as? GridRowCell).map(shownCells.remove)
  }
}

/*
 * Travellers grouped by the first letter of their name, like the UIKit example's Sections screen:
 * built-in section headers that stick, a count in each section's footer, separators and Auto
 * Layout cells that size themselves. The data source gives no sizes. A header view says what the
 * screen is and a footer view counts everyone.
 */
final class SectionsScreen: NSView, ListScreen, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
  let list = ShadowListKitListView()
  private(set) var sections: [(letter: String, contacts: [Contact])] = []
  private var fresh = 5000
  private let banner = BannerView("Travellers by the first letter of their name")
  private let total = BannerView("")

  var keys: [String] { allContacts.map(\.key) }

  init(count: Int) {
    super.init(frame: .zero)
    list.estimatedItemSize = 64
    list.stickySectionHeaders = true
    list.showsSeparators = true
    list.separatorInsetStart = 68
    list.headerView = banner
    list.footerView = total
    list.register(ContactCell.self, forCellReuseIdentifier: "contact")
    list.dataSource = self
    list.delegate = self
    list.autoresizingMask = [.width, .height]
    addSubview(list)
    group((0..<min(count, 400)).map { Contact(index: $0) })
    list.reloadData()
  }

  required init?(coder: NSCoder) { fatalError() }

  override var isFlipped: Bool { true }

  override func viewDidMoveToWindow() {
    super.viewDidMoveToWindow()
    list.frame = bounds
  }

  func group(_ contacts: [Contact]) {
    let sorted = contacts.sorted { $0.name < $1.name }
    sections = Dictionary(grouping: sorted) { String($0.name.prefix(1)) }
      .map { (letter: $0.key, contacts: $0.value) }
      .sorted { $0.letter < $1.letter }
    total.text = "\(contacts.count) travellers"
  }

  var allContacts: [Contact] { sections.flatMap(\.contacts) }

  func contact(at index: Int) -> Contact { allContacts[index] }

  /*
   * A new traveller lands in its section. The list reads the sections again and keeps its place.
   */
  func addTraveller() {
    var contacts = allContacts
    contacts.append(Contact(index: fresh))
    fresh += 1
    group(contacts)
    list.insertItems(at: IndexSet(integer: 0))
  }

  func prependRows() { addTraveller() }
  func appendRows() { addTraveller() }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { allContacts.count }
  func numberOfSections(in listView: ShadowListKitListView) -> Int { sections.count }
  func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { sections[section].contacts.count }
  func listView(_ listView: ShadowListKitListView, keyForSection section: Int) -> String { sections[section].letter }
  func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? { sections[section].letter }
  func listView(_ listView: ShadowListKitListView, titleForFooterInSection section: Int) -> String? {
    "\(sections[section].contacts.count) travellers"
  }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { contact(at: index).key }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "contact") as! ContactCell
    let contact = contact(at: index)
    // Every fifth traveller has a longer note, which wraps and makes the row taller.
    cell.show(
      contact,
      note: index % 5 == 0 ? "\(contact.subtitle) · Window seat, travelling with two bags and a guitar case that goes in the cabin." : contact.subtitle)
    return cell
  }

  func listView(_ listView: ShadowListKitListView, typeSelectStringForItemAt index: Int) -> String? { contact(at: index).name }
}
