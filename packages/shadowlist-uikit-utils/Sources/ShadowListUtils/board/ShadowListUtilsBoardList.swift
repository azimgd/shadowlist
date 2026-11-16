import ShadowListKit
import UIKit

#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * Application-owned columns on one isolated drag board. onMoveItem requests a change; update
 * columns in that callback to accept it. Header and footer views stay outside the draggable rows.
 */
public final class ShadowListUtilsBoardList<Item>: UIView {
  public var columns: [ShadowListUtilsBoardColumn<Item>] {
    get { storedColumns }
    set { setColumns(newValue, animated: false) }
  }

  public func setColumns(_ columns: [ShadowListUtilsBoardColumn<Item>], animated: Bool) {
    precondition(Set(columns.map(\.key)).count == columns.count, "Board column keys must be unique")
    let previousLast = storedColumns.last?.key
    storedColumns = columns
    for key in Array(columnViews.keys) where !columns.contains(where: { $0.key == key }) {
      columnViews.removeValue(forKey: key)?.removeFromSuperview()
    }
    for column in columns {
      let view = columnViews[column.key] ?? makeColumn(column.key)
      view.list.theme = theme
      view.list.backgroundColor = theme.colors.elevated
      view.list.dragBoardId = boardId
      view.list.dragListId = listId(column.key)
      view.list.dragListLabel = column.title
      view.list.reorderEnabled = onMoveItem != nil
      let rows = column.items.map { item -> ShadowListUtilsRow in
        let row = rowForItem(item)
        precondition(row.key == keyExtractor(item), "A board row must use its item's key")
        return row
      }
      view.list.setRows(rows, animated: animated)
      let header: UIView?
      if let renderColumnHeader {
        header = renderColumnHeader(column)
      } else {
        let title = ShadowListUtilsListHeader(title: column.title, subtitle: String(column.items.count), theme: theme)
        title.isAccessibilityElement = true
        title.accessibilityTraits = .header
        title.accessibilityLabel = labels.columnSummary(column.title, column.items.count)
        header = title
      }
      view.show(header: header, footer: renderColumnFooter?(column))
      view.empty.text = onMoveItem == nil ? labels.emptyColumnReadOnly : labels.emptyColumn
      view.empty.isHidden = !column.items.isEmpty
      view.empty.textColor = theme.colors.secondaryLabel
      view.backgroundColor = theme.colors.elevated
      view.layer.cornerRadius = theme.radius.lg
      view.clipsToBounds = true
    }
    columnSource.keys = columns.map(\.key)
    columnSource.views = columns.compactMap { columnViews[$0.key] }
    scroll.reloadData()
    if previousLast != columns.last?.key {
      let changed = Set([previousLast, columns.last?.key].compactMap { $0 })
      scroll.reloadItems(at: IndexSet(columns.indices.filter { changed.contains(columns[$0].key) }))
    }
    setNeedsLayout()
    invalidateIntrinsicContentSize()
  }

  public var onMoveItem: ((ShadowListUtilsBoardMoveInfo<Item>) -> Void)? {
    didSet { setColumns(storedColumns, animated: false) }
  }
  public var rowForItem: (Item) -> ShadowListUtilsRow {
    didSet { setColumns(storedColumns, animated: false) }
  }
  public var renderColumnHeader: ((ShadowListUtilsBoardColumn<Item>) -> UIView?)? {
    didSet { setColumns(storedColumns, animated: false) }
  }
  public var renderColumnFooter: ((ShadowListUtilsBoardColumn<Item>) -> UIView?)? {
    didSet { setColumns(storedColumns, animated: false) }
  }
  public var labels = ShadowListUtilsBoardLabels() {
    didSet { setColumns(storedColumns, animated: false) }
  }
  public var theme: ShadowListUtilsTheme {
    didSet { setColumns(storedColumns, animated: false) }
  }
  public var columnWidth: CGFloat? { didSet { setNeedsLayout() } }
  public var sidePadding: CGFloat? { didSet { setNeedsLayout() } }
  public var columnHeight = ShadowListUtilsBoardColumnHeight.fill { didSet { setNeedsLayout(); invalidateIntrinsicContentSize() } }
  public var minColumnHeight: CGFloat = 120 { didSet { setNeedsLayout(); invalidateIntrinsicContentSize() } }
  public var maxColumnHeight: CGFloat? { didSet { setNeedsLayout(); invalidateIntrinsicContentSize() } }
  public var estimatedItemHeight: CGFloat = 64 { didSet { setNeedsLayout(); invalidateIntrinsicContentSize() } }

  private let keyExtractor: (Item) -> String
  private let boardId: String
  private var storedColumns: [ShadowListUtilsBoardColumn<Item>] = []
  private let scroll = ShadowListKitListView()
  private let columnSource = ShadowListUtilsBoardColumnSource()
  private var columnViews: [String: ShadowListUtilsBoardColumnView] = [:]
  private var measuredColumnWidth: CGFloat = -1
  private var contentHeight: CGFloat = 0

  public init(idPrefix: String? = nil, theme: ShadowListUtilsTheme = .system, keyExtractor: @escaping (Item) -> String,
              rowForItem: @escaping (Item) -> ShadowListUtilsRow) {
    self.theme = theme
    self.keyExtractor = keyExtractor
    self.rowForItem = rowForItem
    boardId = "\(idPrefix ?? "board")-\(UUID().uuidString)"
    super.init(frame: .zero)
    scroll.showsHorizontalScrollIndicator = false
    scroll.isHorizontal = true
    scroll.snapToItem = true
    scroll.allowsSelection = false
    scroll.contentInsetAdjustmentBehavior = .never
    scroll.register(ShadowListUtilsBoardColumnCell.self, forCellReuseIdentifier: "board-column")
    scroll.dataSource = columnSource
    addSubview(scroll)
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  private func listId(_ key: String) -> String { "\(boardId)/\(key)" }

  private func makeColumn(_ key: String) -> ShadowListUtilsBoardColumnView {
    let view = ShadowListUtilsBoardColumnView(theme: theme)
    view.list.onMoveWithin = { [weak self] source, destination in self?.moveWithin(key, source: source, destination: destination) }
    view.list.onMoveToList = { [weak self] source, listId, destination, destinationKey in
      self?.moveBetween(key, source: source, destinationListId: listId, destination: destination, destinationKey: destinationKey)
    }
    view.list.contentChanged = { [weak self] in self?.setNeedsLayout(); self?.invalidateIntrinsicContentSize() }
    columnViews[key] = view
    return view
  }

  private func moveWithin(_ key: String, source: Int, destination: Int) {
    guard let column = storedColumns.first(where: { $0.key == key }), column.items.indices.contains(source) else { return }
    let item = column.items[source]
    let itemKey = keyExtractor(item)
    let remaining = column.items.filter { keyExtractor($0) != itemKey }
    let destinationKey = remaining.indices.contains(destination) ? keyExtractor(remaining[destination]) : ""
    onMoveItem?(ShadowListUtilsBoardMoveInfo(item: item, sourceColumnKey: key, destinationColumnKey: key,
                                           destinationKey: destinationKey, destinationIndex: destination))
  }

  private func moveBetween(_ key: String, source: Int, destinationListId: String, destination: Int, destinationKey: String) {
    guard let column = storedColumns.first(where: { $0.key == key }), column.items.indices.contains(source),
          let target = storedColumns.first(where: { listId($0.key) == destinationListId }) else { return }
    onMoveItem?(ShadowListUtilsBoardMoveInfo(item: column.items[source], sourceColumnKey: key, destinationColumnKey: target.key,
                                           destinationKey: destinationKey, destinationIndex: destination))
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    let layout = ShadowListUtilsCore.boardColumnLayout(forWidth: bounds.width, columnWidth: columnWidth ?? .nan, sidePadding: sidePadding ?? .nan)
    columnSource.columnWidth = layout.columnWidth
    columnSource.gap = layout.snapInterval - layout.columnWidth
    let heights = storedColumns.map { column -> NSNumber in
      let view = columnViews[column.key]!
      let measured = view.list.contentSize.height
      return NSNumber(value: Double(measured > 0 ? measured : CGFloat(column.items.count) * estimatedItemHeight))
    }
    let bodyHeight = ShadowListUtilsCore.boardColumnHeight(forHeights: heights, minimum: minColumnHeight,
                                                         maximum: maxColumnHeight ?? (window?.bounds.height ?? UIScreen.main.bounds.height) * 0.6)
    var tallest: CGFloat = 0
    for column in storedColumns {
      guard let view = columnViews[column.key] else { continue }
      let headerHeight = view.header?.sizeThatFits(CGSize(width: layout.columnWidth, height: .greatestFiniteMagnitude)).height ?? 0
      let footerHeight = view.footer?.sizeThatFits(CGSize(width: layout.columnWidth, height: .greatestFiniteMagnitude)).height ?? 0
      let body = columnHeight == .content ? bodyHeight : max(0, bounds.height - theme.spacing.xl - headerHeight - footerHeight)
      let height = headerHeight + body + footerHeight
      view.frame = CGRect(x: 0, y: 0, width: layout.columnWidth, height: height)
      view.header?.frame = CGRect(x: 0, y: 0, width: layout.columnWidth, height: headerHeight)
      view.list.frame = CGRect(x: 0, y: headerHeight, width: layout.columnWidth, height: body)
      view.list.isScrollEnabled = columnHeight == .fill || view.list.contentSize.height > body
      view.footer?.frame = CGRect(x: 0, y: headerHeight + body, width: layout.columnWidth, height: footerHeight)
      view.empty.frame = CGRect(x: 12, y: headerHeight + 24, width: max(0, layout.columnWidth - 24), height: 24)
      tallest = max(tallest, height)
    }
    let crossSize = columnHeight == .content ? tallest : bounds.height
    scroll.frame = CGRect(x: 0, y: 0, width: bounds.width, height: crossSize)
    if scroll.contentInset.left != layout.sidePadding || scroll.contentInset.right != layout.sidePadding {
      let logicalOffset = scroll.contentOffset.x + scroll.contentInset.left
      scroll.contentInset = UIEdgeInsets(top: 0, left: layout.sidePadding, bottom: 0, right: layout.sidePadding)
      scroll.contentOffset.x = logicalOffset - layout.sidePadding
    }
    /*
     * React Native keeps every board column attached. The outer host covers the whole column
     * strip with its existing overscan settings; each inner list keeps its bounded row range.
     */
    let strip = CGFloat(storedColumns.count) * layout.snapInterval + layout.sidePadding * 2
    let coverage = max(1, ceil(strip / max(1, bounds.width)) + 1)
    if scroll.overscan != coverage { scroll.overscan = coverage }
    if scroll.mountOverscan != coverage { scroll.mountOverscan = coverage }
    if measuredColumnWidth != layout.columnWidth {
      measuredColumnWidth = layout.columnWidth
      scroll.reloadItems(at: IndexSet(integersIn: 0..<storedColumns.count))
    }
    scroll.layoutIfNeeded()
    if contentHeight != tallest { contentHeight = tallest; invalidateIntrinsicContentSize() }
  }

  public override var intrinsicContentSize: CGSize {
    CGSize(width: UIView.noIntrinsicMetric, height: columnHeight == .content ? contentHeight : UIView.noIntrinsicMetric)
  }

  func list(forColumn key: String) -> ShadowListUtilsList? { columnViews[key]?.list }
}

private final class ShadowListUtilsBoardColumnView: UIView {
  let list: ShadowListUtilsBoardColumnList
  let empty = UILabel()
  private(set) var header: UIView?
  private(set) var footer: UIView?

  init(theme: ShadowListUtilsTheme) {
    list = ShadowListUtilsBoardColumnList(theme: theme)
    super.init(frame: .zero)
    list.allowsSelection = false
    empty.textAlignment = .center
    empty.isUserInteractionEnabled = false
    addSubview(list)
    addSubview(empty)
  }
  required init?(coder: NSCoder) { fatalError() }
  func show(header: UIView?, footer: UIView?) {
    if self.header !== header { self.header?.removeFromSuperview(); self.header = header; if let header { addSubview(header) } }
    if self.footer !== footer { self.footer?.removeFromSuperview(); self.footer = footer; if let footer { addSubview(footer) } }
  }
}

private final class ShadowListUtilsBoardColumnList: ShadowListUtilsList {
  var onMoveWithin: ((Int, Int) -> Void)?
  var onMoveToList: ((Int, String, Int, String) -> Void)?
  var contentChanged: (() -> Void)?
  private var previousContentSize = CGSize.zero

  override func moveItem(from sourceIndex: Int, to destinationIndex: Int) { onMoveWithin?(sourceIndex, destinationIndex) }
  override func moveItem(from sourceIndex: Int, toListId: String, destinationIndex: Int, destinationKey: String) {
    onMoveToList?(sourceIndex, toListId, destinationIndex, destinationKey)
  }
  override func layoutSubviews() {
    super.layoutSubviews()
    if previousContentSize != contentSize { previousContentSize = contentSize; contentChanged?() }
  }
}

/*
 * A normal horizontal kit data source: the shared snap target is each column's leading edge.
 */
private final class ShadowListUtilsBoardColumnSource: NSObject, ShadowListKitListViewDataSource {
  var keys: [String] = []
  var views: [UIView] = []
  var columnWidth: CGFloat = 280
  var gap: CGFloat = 12

  func numberOfItems(in listView: ShadowListKitListView) -> Int { keys.count }
  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { keys[index] }
  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
    columnWidth + (index + 1 < keys.count ? gap : 0)
  }
  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "board-column") as! ShadowListUtilsBoardColumnCell
    cell.show(views[index], width: columnWidth)
    return cell
  }
}

private final class ShadowListUtilsBoardColumnCell: ShadowListKitListCell {
  private var column: UIView?
  private var columnWidth: CGFloat = 0

  func show(_ column: UIView, width: CGFloat) {
    if self.column !== column {
      if self.column?.superview === self { self.column?.removeFromSuperview() }
      self.column = column
      addSubview(column)
    }
    columnWidth = width
    setNeedsLayout()
  }
  override func layoutSubviews() {
    super.layoutSubviews()
    if column?.superview === self { column?.frame = CGRect(x: 0, y: 0, width: columnWidth, height: column?.frame.height ?? bounds.height) }
  }
  override func prepareForReuse() {
    if column?.superview === self { column?.removeFromSuperview() }
    column = nil
    super.prepareForReuse()
  }
}
