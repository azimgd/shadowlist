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
 * itself.
 */
final class FeedScreen: NSView, ListScreen, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
  let list = ShadowListKitListView()
  private var posts: [FeedPost]
  private var layouts: [String: FeedLayout] = [:]
  private var nextIndex: Int
  private var previousIndex = -1

  var keys: [String] { posts.map(\.key) }

  init(count: Int) {
    posts = (0..<count).map { FeedPost(index: $0) }
    nextIndex = count
    super.init(frame: .zero)
    list.showsSeparators = true
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
}

/*
 * A host that moves a driven list the way a two axis grid does. The wheel scrolls both axes:
 * down the rows through the list's contentOffset and scrollPhase, sideways by shifting every
 * row's tiles. Offsets the list asks for land at once. Some rows are taller, as wrapped cells
 * would make them.
 */
final class DrivenScreen: NSView, ListScreen, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
  static let columnCount = 40

  let list = ShadowListKitListView()
  private(set) var rows: [String]
  private(set) var columnOffset: CGFloat = 0
  private(set) var corrections = 0
  private var previousIndex = -1

  var keys: [String] { rows }

  init(count: Int) {
    rows = (0..<count).map { "row-\($0)" }
    super.init(frame: .zero)
    list.scrollDriven = true
    list.allowsSelection = false
    list.estimatedItemSize = 24
    list.register(GridRowCell.self, forCellReuseIdentifier: "row")
    list.dataSource = self
    list.delegate = self
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

  var maxColumnOffset: CGFloat {
    max(0, GridRowCell.numberWidth + CGFloat(DrivenScreen.columnCount) * GridRowCell.columnWidth - list.bounds.width)
  }

  var maxRowOffset: CGFloat {
    max(0, list.contentSize.height - list.bounds.height)
  }

  /*
   * Move both axes, the way the grid's own scroll engine would each frame.
   */
  func scroll(to point: CGPoint) {
    columnOffset = min(max(0, point.x), maxColumnOffset)
    list.contentOffset = CGPoint(x: 0, y: min(max(0, point.y), maxRowOffset))
    for case let cell as GridRowCell in list.visibleCells {
      cell.shift = columnOffset
    }
  }

  override func scrollWheel(with event: NSEvent) {
    switch (event.phase, event.momentumPhase) {
    case (.began, _), (.mayBegin, _): list.scrollPhase = .dragging
    case (_, .began): list.scrollPhase = .settling
    default: break
    }
    let scale: CGFloat = event.hasPreciseScrollingDeltas ? 1 : 16
    scroll(to: CGPoint(x: columnOffset - event.scrollingDeltaX * scale, y: list.contentOffset.y - event.scrollingDeltaY * scale))
    if event.phase == .ended || event.phase == .cancelled || event.momentumPhase == .ended || event.momentumPhase == .cancelled {
      list.scrollPhase = .idle
    }
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
  }

  func listView(_ listView: ShadowListKitListView, willDisplay cell: ShadowListKitListCell, at index: Int) {
    (cell as? GridRowCell)?.shift = columnOffset
  }
}
