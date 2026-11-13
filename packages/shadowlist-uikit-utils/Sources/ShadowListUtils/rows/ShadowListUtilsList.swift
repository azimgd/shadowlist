import ShadowListKit
import UIKit

/*
 * A ShadowListKitListView that shows rows. It is its own data source and delegate through one
 * adapter, sizes rows from their layouts and gives the app closures in place of a delegate.
 * Every property of the kit list stays reachable. Setting dataSource or delegate from outside
 * is a programmer error.
 */
open class ShadowListUtilsList: ShadowListKitListView {
  /*
   * The theme the list draws its background with and the template lists build their rows with.
   * Setting it lays every row out again.
   */
  public var theme: ShadowListUtilsTheme {
    didSet {
      if theme != oldValue {
        reloadRows()
      }
    }
  }

  public private(set) var rows: [ShadowListUtilsRow] = []

  /*
   * Run when the scroll position comes near an edge.
   */
  public var onEndReached: (() -> Void)?
  public var onStartReached: (() -> Void)?

  /*
   * Setting a closure turns pull to refresh on, and nil turns it off. Set the kit's refreshing
   * back to false when the refresh is done.
   */
  public var onRefresh: (() -> Void)? {
    didSet { refreshEnabled = onRefresh != nil }
  }

  /*
   * Runs on every change of the offset.
   */
  public var onScroll: (() -> Void)?

  let layoutCache = ShadowListUtilsLayoutCache()

  /*
   * Where the horizontal lists of nested rows were when their rows left the screen, by row key.
   */
  var nestedAnchors: [String: ShadowListKitAnchorState] = [:]

  /*
   * The cross size the kit last sized an item for: the list's width over its columns, or its
   * height when horizontal. 0 before the first layout.
   */
  var itemCrossSize: CGFloat = 0 {
    didSet {
      guard itemCrossSize != oldValue else { return }
      layoutCache.invalidateAll()
      setNeedsPrefetch()
    }
  }

  private(set) var adapter: ShadowListUtilsListAdapter?

  /*
   * Mixed into every row's content version. A change of it configures every cell again, like
   * after a new theme.
   */
  private var contentGeneration = 0

  private var prefetchScheduled = false
  private var prefetchOffset: CGFloat = 0

  public init(theme: ShadowListUtilsTheme = .system) {
    self.theme = theme
    super.init(frame: .zero)
    ShadowListUtilsPlatform.resolve()
    let adapter = makeAdapter()
    adapter.list = self
    self.adapter = adapter
    super.dataSource = adapter
    super.delegate = adapter
    applyTheme()
    NotificationCenter.default.addObserver(
      self, selector: #selector(textSizeChanged(_:)), name: UIContentSizeCategory.didChangeNotification, object: nil)
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  open override func didMoveToWindow() {
    super.didMoveToWindow()
    if window == nil {
      layoutCache.cancelPrefetch()
    }
  }

  // MARK: - Delegate

  /*
   * UIKit clears a scroll view's delegate when the view goes away, which is why nil passes.
   */
  open override var dataSource: ShadowListKitListViewDataSource? {
    didSet {
      guard let dataSource, dataSource !== adapter else { return }
      assertionFailure("A ShadowListUtilsList is its own data source. Use a ShadowListKitListView with the library's rows instead.")
      super.dataSource = adapter
    }
  }

  open override var delegate: ShadowListKitListViewDelegate? {
    didSet {
      guard let delegate, delegate !== adapter else { return }
      assertionFailure("A ShadowListUtilsList is its own delegate. Use a ShadowListKitListView with the library's rows instead.")
      super.delegate = adapter
    }
  }

  // MARK: - Data

  /*
   * Show these rows. The list diffs them by key against the ones it shows and configures the
   * cells whose content version changed. animated animates this change.
   */
  public func setRows(_ rows: [ShadowListUtilsRow], animated: Bool = false) {
    let previous = self.rows
    self.rows = rows
    layoutCache.prune(keeping: Set((rows + (adapter?.headerRows ?? [])).map(\.key)))
    let animates = animatesChanges
    animatesChanges = animated
    if let inserted = insertedIndices(from: previous, to: rows) {
      insertItems(at: inserted)
    } else {
      applyChanges()
    }
    animatesChanges = animates
    replaceMountedRows()
    setNeedsPrefetch()
  }

  /*
   * Stores rows the kit already holds in this order, like after a drop.
   */
  func replaceRows(_ rows: [ShadowListUtilsRow]) {
    self.rows = rows
  }

  /*
   * Where the new rows sit when the rows before are all still there, unchanged and in one run
   * at the start or the end: a pure append or prepend, which needs no diff. nil for any other
   * change, and for a list with sections, whose counts change with the rows.
   */
  private func insertedIndices(from previous: [ShadowListUtilsRow], to rows: [ShadowListUtilsRow]) -> IndexSet? {
    guard !previous.isEmpty, rows.count > previous.count, adapter?.headerRows.isEmpty != false else { return nil }
    let added = rows.count - previous.count
    func matches(offset: Int) -> Bool {
      for index in previous.indices {
        let row = rows[index + offset]
        if row.key != previous[index].key || row.contentVersion != previous[index].contentVersion {
          return false
        }
      }
      return true
    }
    if matches(offset: 0) {
      return IndexSet(integersIn: previous.count..<rows.count)
    }
    return matches(offset: added) ? IndexSet(integersIn: 0..<added) : nil
  }

  /*
   * A mounted cell keeps its row object while the content version stays. Hand it the row built
   * now, which may carry a new closure.
   */
  private func replaceMountedRows() {
    for case let view as ShadowListUtilsRowView in visibleCells {
      let index = view.index
      if index != NSNotFound, rows.indices.contains(index), rows[index].key == view.row?.key {
        view.replaceRow(rows[index])
      }
    }
  }

  /*
   * The content version the kit compares for a row: the row's own under the list's generation.
   */
  func contentVersion(of row: ShadowListUtilsRow) -> Int {
    (contentGeneration << Self.generationShift) | Int(UInt32(truncatingIfNeeded: row.contentVersion))
  }

  // MARK: - Template hooks

  /*
   * Called once, from the list's initializer.
   */
  func makeAdapter() -> ShadowListUtilsListAdapter {
    ShadowListUtilsListAdapter()
  }

  /*
   * Build the rows again from the template's items, after the theme or an option changed.
   * Returns nil for a list whose rows the app sets.
   */
  func makeRows() -> [ShadowListUtilsRow]? {
    nil
  }

  /*
   * Show the template's rows for its items, and let the kit diff them against the rows before.
   */
  func rebuildRows(animated: Bool = false) {
    setRows(makeRows() ?? rows, animated: animated)
  }

  /*
   * Everything the cells show may differ, like after a new theme, new labels or a new text
   * size: drop the layouts, build every row again and configure every cell again.
   */
  func reloadRows() {
    layoutCache.invalidateAll()
    contentGeneration += 1
    applyTheme()
    rebuildRows()
  }

  /*
   * Set what the list itself takes from the theme.
   */
  func applyTheme() {
    backgroundColor = theme.colors.background
  }

  func didSelectItem(at index: Int) {}

  func didDeselectItem(at index: Int) {}

  func didLongPressItem(at index: Int) {}

  /*
   * A held item was dropped at a new place. The kit reads the rows again right after.
   */
  func moveItem(from sourceIndex: Int, to destinationIndex: Int) {
    rows = ShadowListUtilsArrayMove.moved(rows, from: sourceIndex, to: destinationIndex)
  }

  func leadingSwipeActions(at index: Int) -> ShadowListKitSwipeActionsConfiguration? {
    nil
  }

  func trailingSwipeActions(at index: Int) -> ShadowListKitSwipeActionsConfiguration? {
    nil
  }

  func contextMenu(at index: Int) -> UIMenu? {
    nil
  }

  /*
   * What a screen reader can do with an item besides pressing it: the template's closures that
   * are otherwise behind a gesture.
   */
  func moveItem(from sourceIndex: Int, toListId: String, destinationIndex: Int, destinationKey: String) {}

  func accessibilityActions(at index: Int) -> [UIAccessibilityCustomAction] {
    []
  }

  // MARK: - Text size

  /*
   * The reader chose a new text size. Every layout is made again for it.
   */
  @objc private func textSizeChanged(_ notification: Notification) {
    let category = notification.userInfo?[UIContentSizeCategory.newValueUserInfoKey] as? UIContentSizeCategory
    ShadowListUtilsPlatform.resolve(category: category, window: window)
    reloadRows()
  }

  // MARK: - Prefetching

  /*
   * Lay out the rows around the first visible one on the cache's queue, once per run loop turn.
   */
  private func setNeedsPrefetch() {
    guard !prefetchScheduled else { return }
    prefetchScheduled = true
    DispatchQueue.main.async { [weak self] in
      guard let self else { return }
      self.prefetchScheduled = false
      guard self.itemCrossSize > 0 else { return }
      let range = self.visibleRange
      self.prefetchOffset = self.offsetAlong
      self.layoutCache.prefetch(self.rows, width: self.itemCrossSize, around: range.location == NSNotFound ? 0 : range.location)
    }
  }

  func didScroll() {
    onScroll?()
    let viewport = isHorizontal ? bounds.width : bounds.height
    if abs(offsetAlong - prefetchOffset) > viewport / 2 {
      setNeedsPrefetch()
    }
  }

  private var offsetAlong: CGFloat {
    isHorizontal ? contentOffset.x : contentOffset.y
  }

  private static let generationShift = 32
}

// MARK: - Items

/*
 * What every template list does with its items.
 */
extension ShadowListUtilsList {
  /*
   * The rows of the items. An item that equals the one shown under its key keeps its row,
   * which the list then neither builds nor lays out again.
   */
  func rows<Item: Equatable>(
    for items: [Item], previousItems: [Item], previousRows: [ShadowListUtilsRow], key: (Item) -> String,
    make: (Item) -> ShadowListUtilsRow
  ) -> [ShadowListUtilsRow] {
    guard previousItems.count == previousRows.count, !previousItems.isEmpty else { return items.map(make) }
    var positions: [String: Int] = [:]
    positions.reserveCapacity(previousItems.count)
    for (position, item) in previousItems.enumerated() {
      positions[key(item)] = position
    }
    return items.map { item in
      let itemKey = key(item)
      if let position = positions[itemKey], previousItems[position] == item, previousRows[position].key == itemKey {
        return previousRows[position]
      }
      return make(item)
    }
  }
}
