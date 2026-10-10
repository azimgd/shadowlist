import ShadowListKit
import UIKit

/*
 * What changed between two row lists. UITableView needs it to keep the visible rows still;
 * ShadowListKitListView works it out from the keys.
 */
enum RowChange {
  case reset
  case prepend(Int)
  case append(Int)
  case update
}

/*
 * One list API over the engines being compared. Screens hand it rows and it shows them with
 * ShadowListKitListView, UITableView, or for a grid UICollectionView.
 */
final class ListController: NSObject {
  let engine: Engine
  let columns: Int
  let cache = LayoutCache()
  private(set) var rows: [Row] = []

  var onStartReached: (() -> Void)?
  var onEndReached: (() -> Void)?

  private var backend: ListBackend!

  init(engine: Engine, columns: Int = 1, inverted: Bool = false) {
    self.engine = engine
    self.columns = columns
    super.init()
    if engine.isShadowList {
      backend = ShadowListBackend(controller: self, inverted: inverted)
    } else if columns > 1 {
      backend = CollectionBackend(controller: self)
    } else {
      backend = TableBackend(controller: self, inverted: inverted)
    }
  }

  var view: UIScrollView { backend.scrollView }

  var header: UIView? {
    get { backend.header }
    set { backend.header = newValue }
  }

  var footer: UIView? {
    get { backend.footer }
    set { backend.footer = newValue }
  }

  var stickyIndices: [Int] = [] {
    didSet { backend.stickyIndicesChanged() }
  }

  /*
   * Called after a dragged row was dropped and rows already holds the new order.
   */
  var onMove: ((Int, Int) -> Void)?

  var reorderEnabled = false {
    didSet { backend.reorderEnabled = reorderEnabled }
  }

  var snapToItem = false {
    didSet { backend.snapToItem = snapToItem }
  }

  func moveRow(from: Int, to: Int) {
    let row = rows.remove(at: from)
    rows.insert(row, at: to)
    onMove?(from, to)
  }

  var rowWidth: CGFloat {
    let width = view.bounds.width - view.adjustedContentInset.left - view.adjustedContentInset.right
    return columns > 1 ? floor(width / CGFloat(columns)) : width
  }

  /*
   * How long the last backend update took on the main thread, for the cost scenario.
   */
  private(set) var lastUpdateSeconds: CFTimeInterval = 0

  func setRows(_ rows: [Row], change: RowChange) {
    self.rows = rows
    if !engine.selfSizing {
      cache.prefetch(rows, width: rowWidth, around: 0)
    }
    let start = CACurrentMediaTime()
    backend.rowsChanged(change)
    lastUpdateSeconds = CACurrentMediaTime() - start
  }

  func reloadRow(at index: Int, _ row: Row) {
    rows[index] = row
    cache.invalidate(row.key)
    backend.reloadRow(at: index)
  }

  func scrollToIndex(_ index: Int, animated: Bool) {
    backend.scrollToIndex(index, animated: animated)
  }

  func scrollToEnd(animated: Bool) {
    backend.scrollToEnd(animated: animated)
  }

  /*
   * The layout a cell shows, or nil when the engine sizes rows from their views.
   */
  func cachedLayout(_ row: Row, width: CGFloat) -> RowLayout? {
    engine.selfSizing ? nil : cache.layout(row, width: width)
  }
}

protocol ListBackend: AnyObject {
  var scrollView: UIScrollView { get }
  var reorderEnabled: Bool { get set }
  var snapToItem: Bool { get set }
  var header: UIView? { get set }
  var footer: UIView? { get set }
  func rowsChanged(_ change: RowChange)
  func reloadRow(at index: Int)
  func stickyIndicesChanged()
  func scrollToIndex(_ index: Int, animated: Bool)
  func scrollToEnd(animated: Bool)
}

// MARK: - ShadowListKitListView

final class ShadowListCell: ShadowListKitListCell {
  private(set) var rowView: RowView?

  func show(_ row: Row, layout: RowLayout?) {
    if rowView == nil || type(of: rowView!) != row.viewClass {
      rowView?.removeFromSuperview()
      let view = row.viewClass.init(frame: bounds)
      addSubview(view)
      rowView = view
    }
    rowView!.configure(row, layout: layout)
  }

  override func sizeThatFits(_ size: CGSize) -> CGSize {
    rowView?.sizeThatFits(size) ?? .zero
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    rowView?.frame = bounds
  }
}

final class ShadowListBackend: NSObject, ListBackend, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
  private unowned let controller: ListController
  private let list = ShadowListKitListView()
  private var registered = Set<String>()

  init(controller: ListController, inverted: Bool) {
    self.controller = controller
    super.init()
    list.backgroundColor = Theme.background
    list.isInverted = inverted
    list.numberOfColumns = controller.columns
    list.alwaysBounceVertical = true
    list.dataSource = self
    list.delegate = self
    list.estimatedItemSize = 120
  }

  var scrollView: UIScrollView { list }

  var reorderEnabled: Bool {
    get { list.reorderEnabled }
    set { list.reorderEnabled = newValue }
  }

  var snapToItem: Bool {
    get { list.snapToItem }
    set { list.snapToItem = newValue }
  }

  func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, to destinationIndex: Int) {
    controller.moveRow(from: sourceIndex, to: destinationIndex)
  }

  var header: UIView? {
    get { list.headerView }
    set { list.headerView = newValue }
  }

  var footer: UIView? {
    get { list.footerView }
    set { list.footerView = newValue }
  }

  func rowsChanged(_ change: RowChange) {
    switch change {
    case .prepend(let count):
      list.insertItems(at: IndexSet(integersIn: 0..<count))
    case .append(let count):
      let total = controller.rows.count
      list.insertItems(at: IndexSet(integersIn: (total - count)..<total))
    case .reset, .update:
      list.reloadData()
    }
  }

  func reloadRow(at index: Int) {
    list.reloadItems(at: IndexSet(integer: index))
  }

  func stickyIndicesChanged() {
    list.stickyIndices = IndexSet(controller.stickyIndices)
  }

  func scrollToIndex(_ index: Int, animated: Bool) {
    list.scrollToItem(at: index, viewPosition: 0, animated: animated)
  }

  func scrollToEnd(animated: Bool) {
    list.scrollToEnd(animated: animated)
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int {
    controller.rows.count
  }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String {
    controller.rows[index].key
  }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let row = controller.rows[index]
    let identifier = row.reuseIdentifier
    if !registered.contains(identifier) {
      listView.register(ShadowListCell.self, forCellReuseIdentifier: identifier)
      registered.insert(identifier)
    }
    let cell = listView.dequeueReusableCell(withIdentifier: identifier) as! ShadowListCell
    let width = controller.columns > 1 ? controller.rowWidth : listView.bounds.width
    cell.show(row, layout: controller.cachedLayout(row, width: width))
    return cell
  }

  /*
   * The precomputed sizes are only offered in the fast mode. Without them the list measures
   * every row through its cell, and respondsToSelector is what it asks.
   */
  override func responds(to selector: Selector!) -> Bool {
    if selector == #selector(ShadowListKitListViewDataSource.listView(_:sizeForItemAt:crossSize:)) {
      return !controller.engine.selfSizing
    }
    return super.responds(to: selector)
  }

  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
    controller.cache.layout(controller.rows[index], width: crossSize).height
  }

  func listViewDidReachStart(_ listView: ShadowListKitListView) {
    controller.onStartReached?()
  }

  func listViewDidReachEnd(_ listView: ShadowListKitListView) {
    controller.onEndReached?()
  }
}

// MARK: - UITableView

final class TableCell: UITableViewCell {
  private(set) var rowView: RowView?

  override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
    super.init(style: style, reuseIdentifier: reuseIdentifier)
    selectionStyle = .none
    backgroundColor = Theme.background
  }

  required init?(coder: NSCoder) { fatalError() }

  func show(_ row: Row, layout: RowLayout?) {
    if rowView == nil || type(of: rowView!) != row.viewClass {
      rowView?.removeFromSuperview()
      let view = row.viewClass.init(frame: contentView.bounds)
      contentView.addSubview(view)
      rowView = view
    }
    rowView!.configure(row, layout: layout)
  }

  override func systemLayoutSizeFitting(
    _ targetSize: CGSize, withHorizontalFittingPriority horizontal: UILayoutPriority, verticalFittingPriority vertical: UILayoutPriority
  ) -> CGSize {
    rowView?.sizeThatFits(CGSize(width: targetSize.width, height: .greatestFiniteMagnitude)) ?? .zero
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    rowView?.frame = contentView.bounds
  }
}

final class TableHeader: UITableViewHeaderFooterView {
  private(set) var rowView: RowView?

  func show(_ row: Row, layout: RowLayout?) {
    if rowView == nil {
      let view = row.viewClass.init(frame: contentView.bounds)
      contentView.addSubview(view)
      rowView = view
    }
    rowView!.configure(row, layout: layout)
  }

  override func systemLayoutSizeFitting(
    _ targetSize: CGSize, withHorizontalFittingPriority horizontal: UILayoutPriority, verticalFittingPriority vertical: UILayoutPriority
  ) -> CGSize {
    rowView?.sizeThatFits(CGSize(width: targetSize.width, height: .greatestFiniteMagnitude)) ?? .zero
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    rowView?.frame = contentView.bounds
  }
}

final class TableBackend: NSObject, ListBackend, UITableViewDataSource, UITableViewDelegate, UITableViewDragDelegate, UITableViewDropDelegate {
  private unowned let controller: ListController
  private let table = UITableView(frame: .zero, style: .plain)
  private let inverted: Bool
  private var registered = Set<String>()
  private var startReachedArmed = true
  private var endReachedArmed = true

  /*
   * Sections when the list has sticky headers: the header row and the rows under it.
   */
  private var sections: [(header: Int?, rows: Range<Int>)] = []

  init(controller: ListController, inverted: Bool) {
    self.controller = controller
    self.inverted = inverted
    super.init()
    table.backgroundColor = Theme.background
    table.separatorStyle = .none
    table.dataSource = self
    table.delegate = self
    table.sectionHeaderTopPadding = 0
    table.register(TableHeader.self, forHeaderFooterViewReuseIdentifier: "header")
    if controller.engine.selfSizing {
      table.rowHeight = UITableView.automaticDimension
      table.estimatedRowHeight = 120
      table.sectionHeaderHeight = UITableView.automaticDimension
      table.estimatedSectionHeaderHeight = 38
    } else {
      table.estimatedRowHeight = 0
      table.estimatedSectionHeaderHeight = 0
    }
  }

  var scrollView: UIScrollView { table }

  /*
   * Reordering through UITableView's own drag and drop, started by touch and hold.
   */
  var reorderEnabled = false {
    didSet {
      table.dragInteractionEnabled = reorderEnabled
      table.dragDelegate = reorderEnabled ? self : nil
      table.dropDelegate = reorderEnabled ? self : nil
    }
  }

  var snapToItem = false

  func tableView(_ tableView: UITableView, itemsForBeginning session: UIDragSession, at indexPath: IndexPath) -> [UIDragItem] {
    [UIDragItem(itemProvider: NSItemProvider())]
  }

  func tableView(_ tableView: UITableView, dropSessionDidUpdate session: UIDropSession, withDestinationIndexPath destinationIndexPath: IndexPath?) -> UITableViewDropProposal {
    UITableViewDropProposal(operation: .move, intent: .insertAtDestinationIndexPath)
  }

  func tableView(_ tableView: UITableView, performDropWith coordinator: UITableViewDropCoordinator) {}

  func tableView(_ tableView: UITableView, canMoveRowAt indexPath: IndexPath) -> Bool {
    reorderEnabled
  }

  func tableView(_ tableView: UITableView, moveRowAt sourceIndexPath: IndexPath, to destinationIndexPath: IndexPath) {
    controller.moveRow(from: rowIndex(sourceIndexPath), to: rowIndex(destinationIndexPath))
    rebuildSections()
  }

  /*
   * Snapping by hand: rest on the row edge nearest to where the fling would stop.
   */
  func scrollViewWillEndDragging(_ scrollView: UIScrollView, withVelocity velocity: CGPoint, targetContentOffset: UnsafeMutablePointer<CGPoint>) {
    guard snapToItem else { return }
    let inset = table.adjustedContentInset.top
    let target = targetContentOffset.pointee.y + inset
    let probe = CGRect(x: 0, y: target - table.bounds.height, width: 1, height: table.bounds.height * 2)
    var best = target
    var bestDistance = CGFloat.greatestFiniteMagnitude
    for path in table.indexPathsForRows(in: probe) ?? [] {
      let top = table.rectForRow(at: path).minY
      if abs(top - target) < bestDistance {
        bestDistance = abs(top - target)
        best = top
      }
    }
    let maxOffset = max(0, table.contentSize.height - table.bounds.height + table.adjustedContentInset.bottom + inset)
    targetContentOffset.pointee.y = min(best, maxOffset) - inset
  }

  var header: UIView? {
    get { table.tableHeaderView }
    set { table.tableHeaderView = newValue }
  }

  var footer: UIView? {
    get { table.tableFooterView }
    set {
      newValue?.frame.size.width = table.bounds.width
      table.tableFooterView = newValue
    }
  }

  private func rebuildSections() {
    let rows = controller.rows.count
    let sticky = controller.stickyIndices.filter { $0 < rows }
    guard !sticky.isEmpty else {
      sections = [(nil, 0..<rows)]
      return
    }
    sections = []
    if sticky[0] > 0 {
      sections.append((nil, 0..<sticky[0]))
    }
    for (position, header) in sticky.enumerated() {
      let end = position + 1 < sticky.count ? sticky[position + 1] : rows
      sections.append((header, (header + 1)..<end))
    }
  }

  private func rowIndex(_ indexPath: IndexPath) -> Int {
    sections[indexPath.section].rows.lowerBound + indexPath.row
  }

  private func indexPath(_ index: Int) -> IndexPath? {
    for (section, entry) in sections.enumerated() {
      if entry.header == index {
        return IndexPath(row: NSNotFound, section: section)
      }
      if entry.rows.contains(index) {
        return IndexPath(row: index - entry.rows.lowerBound, section: section)
      }
    }
    return nil
  }

  func stickyIndicesChanged() {
    rebuildSections()
    table.reloadData()
  }

  func rowsChanged(_ change: RowChange) {
    rebuildSections()
    switch change {
    case .prepend(let count):
      // Keep the first visible row where it is, the usual UITableView recipe.
      guard let anchor = table.indexPathsForVisibleRows?.first else {
        table.reloadData()
        return
      }
      let before = table.rectForRow(at: anchor).minY - table.contentOffset.y
      table.reloadData()
      table.layoutIfNeeded()
      if let moved = indexPath(rowIndex(anchor) + count), moved.row != NSNotFound {
        table.contentOffset.y = table.rectForRow(at: moved).minY - before
      }
    case .reset:
      table.reloadData()
      if inverted, let last = lastIndexPath() {
        table.layoutIfNeeded()
        table.scrollToRow(at: last, at: .bottom, animated: false)
        // Self-sizing rows land on estimates first. A second pass lands on the real end.
        table.layoutIfNeeded()
        table.scrollToRow(at: last, at: .bottom, animated: false)
      }
    case .append, .update:
      table.reloadData()
    }
    startReachedArmed = true
    endReachedArmed = true
  }

  func reloadRow(at index: Int) {
    if let path = indexPath(index), path.row != NSNotFound {
      table.reloadRows(at: [path], with: .none)
    }
  }

  private func lastIndexPath() -> IndexPath? {
    guard let section = sections.lastIndex(where: { !$0.rows.isEmpty }) else { return nil }
    return IndexPath(row: sections[section].rows.count - 1, section: section)
  }

  func scrollToIndex(_ index: Int, animated: Bool) {
    guard let path = indexPath(index) else { return }
    if path.row == NSNotFound {
      table.scrollToRow(at: IndexPath(row: NSNotFound, section: path.section), at: .top, animated: animated)
    } else {
      table.scrollToRow(at: path, at: .top, animated: animated)
    }
  }

  func scrollToEnd(animated: Bool) {
    if let last = lastIndexPath() {
      table.scrollToRow(at: last, at: .bottom, animated: animated)
    }
  }

  func numberOfSections(in tableView: UITableView) -> Int {
    sections.count
  }

  func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    sections[section].rows.count
  }

  func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
    let row = controller.rows[rowIndex(indexPath)]
    let identifier = row.reuseIdentifier
    if !registered.contains(identifier) {
      tableView.register(TableCell.self, forCellReuseIdentifier: identifier)
      registered.insert(identifier)
    }
    let cell = tableView.dequeueReusableCell(withIdentifier: identifier, for: indexPath) as! TableCell
    cell.show(row, layout: controller.cachedLayout(row, width: tableView.bounds.width))
    return cell
  }

  func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
    if controller.engine.selfSizing {
      return UITableView.automaticDimension
    }
    return controller.cache.layout(controller.rows[rowIndex(indexPath)], width: tableView.bounds.width).height
  }

  func tableView(_ tableView: UITableView, viewForHeaderInSection section: Int) -> UIView? {
    guard let header = sections[section].header else { return nil }
    let view = tableView.dequeueReusableHeaderFooterView(withIdentifier: "header") as! TableHeader
    let row = controller.rows[header]
    view.show(row, layout: controller.cachedLayout(row, width: tableView.bounds.width))
    return view
  }

  func tableView(_ tableView: UITableView, heightForHeaderInSection section: Int) -> CGFloat {
    guard let header = sections[section].header else { return 0 }
    if controller.engine.selfSizing {
      return UITableView.automaticDimension
    }
    return controller.cache.layout(controller.rows[header], width: tableView.bounds.width).height
  }

  func scrollViewDidScroll(_ scrollView: UIScrollView) {
    let offset = scrollView.contentOffset.y + scrollView.adjustedContentInset.top
    let window = scrollView.bounds.height
    let maxOffset = scrollView.contentSize.height - window + scrollView.adjustedContentInset.bottom
    if offset < window {
      if startReachedArmed {
        startReachedArmed = false
        controller.onStartReached?()
      }
    } else {
      startReachedArmed = true
    }
    if offset > maxOffset - window {
      if endReachedArmed {
        endReachedArmed = false
        controller.onEndReached?()
      }
    } else {
      endReachedArmed = true
    }
  }
}

// MARK: - UICollectionView grid

/*
 * Round robin columns like the core: item i goes into column i % columns. Every frame is
 * worked out up front in prepare, the usual custom layout recipe.
 */
final class ColumnsLayout: UICollectionViewLayout {
  unowned var controller: ListController!
  private var frames: [CGRect] = []
  private var contentHeight: CGFloat = 0
  var footerHeight: CGFloat = 0

  override func prepare() {
    guard let collectionView else { return }
    let columns = controller.columns
    let width = controller.rowWidth
    var heights = [CGFloat](repeating: 0, count: columns)
    frames = controller.rows.enumerated().map { index, row in
      let column = index % columns
      let height = controller.engine.selfSizing
        ? row.layout(width: width).height
        : controller.cache.layout(row, width: width).height
      let frame = CGRect(x: CGFloat(column) * width, y: heights[column], width: width, height: height)
      heights[column] += height
      return frame
    }
    contentHeight = (heights.max() ?? 0) + footerHeight
    _ = collectionView
  }

  override var collectionViewContentSize: CGSize {
    CGSize(width: collectionView?.bounds.width ?? 0, height: contentHeight)
  }

  override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]? {
    var result: [UICollectionViewLayoutAttributes] = []
    for (index, frame) in frames.enumerated() where frame.intersects(rect) {
      let attributes = UICollectionViewLayoutAttributes(forCellWith: IndexPath(item: index, section: 0))
      attributes.frame = frame
      result.append(attributes)
    }
    return result
  }

  override func layoutAttributesForItem(at indexPath: IndexPath) -> UICollectionViewLayoutAttributes? {
    let attributes = UICollectionViewLayoutAttributes(forCellWith: indexPath)
    attributes.frame = frames[indexPath.item]
    return attributes
  }

  override func shouldInvalidateLayout(forBoundsChange newBounds: CGRect) -> Bool {
    newBounds.width != collectionView?.bounds.width
  }
}

final class GridCell: UICollectionViewCell {
  private(set) var rowView: RowView?

  func show(_ row: Row, layout: RowLayout?) {
    if rowView == nil {
      let view = row.viewClass.init(frame: contentView.bounds)
      contentView.addSubview(view)
      rowView = view
    }
    rowView!.configure(row, layout: layout)
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    rowView?.frame = contentView.bounds
  }
}

final class CollectionBackend: NSObject, ListBackend, UICollectionViewDataSource, UICollectionViewDelegate {
  private unowned let controller: ListController
  private let layout = ColumnsLayout()
  private let collection: UICollectionView
  private var endReachedArmed = true

  init(controller: ListController) {
    self.controller = controller
    collection = UICollectionView(frame: .zero, collectionViewLayout: layout)
    super.init()
    layout.controller = controller
    collection.backgroundColor = Theme.background
    collection.dataSource = self
    collection.delegate = self
    collection.register(GridCell.self, forCellWithReuseIdentifier: "cell")
  }

  var scrollView: UIScrollView { collection }

  var reorderEnabled = false
  var snapToItem = false

  var header: UIView?

  var footer: UIView? {
    didSet {
      oldValue?.removeFromSuperview()
      if let footer {
        collection.addSubview(footer)
        layout.footerHeight = footer.frame.height
      }
    }
  }

  func rowsChanged(_ change: RowChange) {
    if case .prepend(let count) = change, let first = collection.indexPathsForVisibleItems.min() {
      let before = (layout.layoutAttributesForItem(at: first)?.frame.minY ?? 0) - collection.contentOffset.y
      collection.reloadData()
      collection.layoutIfNeeded()
      let moved = IndexPath(item: first.item + count, section: 0)
      collection.contentOffset.y = (layout.layoutAttributesForItem(at: moved)?.frame.minY ?? 0) - before
    } else {
      collection.reloadData()
    }
    endReachedArmed = true
  }

  func reloadRow(at index: Int) {
    collection.reloadItems(at: [IndexPath(item: index, section: 0)])
  }

  func stickyIndicesChanged() {}

  func scrollToIndex(_ index: Int, animated: Bool) {
    collection.scrollToItem(at: IndexPath(item: index, section: 0), at: .top, animated: animated)
  }

  func scrollToEnd(animated: Bool) {
    let count = controller.rows.count
    if count > 0 {
      collection.scrollToItem(at: IndexPath(item: count - 1, section: 0), at: .bottom, animated: animated)
    }
  }

  func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
    controller.rows.count
  }

  func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
    let cell = collectionView.dequeueReusableCell(withReuseIdentifier: "cell", for: indexPath) as! GridCell
    let row = controller.rows[indexPath.item]
    cell.show(row, layout: controller.cachedLayout(row, width: controller.rowWidth))
    return cell
  }

  func scrollViewDidScroll(_ scrollView: UIScrollView) {
    if let footer {
      footer.frame = CGRect(x: 0, y: scrollView.contentSize.height - footer.frame.height, width: scrollView.bounds.width, height: footer.frame.height)
    }
    let offset = scrollView.contentOffset.y + scrollView.adjustedContentInset.top
    let window = scrollView.bounds.height
    if offset > scrollView.contentSize.height - 2 * window {
      if endReachedArmed {
        endReachedArmed = false
        controller.onEndReached?()
      }
    } else {
      endReachedArmed = true
    }
  }
}
