import ShadowListKit
import UIKit

/*
 * Contacts in sections with headers that stick to the top. Each header shows the section's
 * title and how many contacts it holds. onPressItem, rowForItem, onDelete and
 * disclosureIndicator are those of ShadowListUtilsContactsList.
 */
public final class ShadowListUtilsContactsSectionList: ShadowListUtilsList {
  /*
   * The groups. Setting them shows them without an animation.
   */
  public var sections: [ShadowListUtilsContactSection] {
    get { storedSections }
    set { setSections(newValue, animated: false) }
  }

  /*
   * Show these sections. animated animates the change from the sections before.
   */
  public func setSections(_ sections: [ShadowListUtilsContactSection], animated: Bool) {
    let items = sections.flatMap(\.items)
    let rows = rows(for: items, previousItems: storedItems, previousRows: rows, key: \.key, make: makeRow)
    storedSections = sections
    storedItems = items
    showRows(rows, animated: animated)
  }

  /*
   * The contacts of every section in order. Setting them groups them with
   * shadowListUtilsGroupIntoSections.
   */
  public var items: [ShadowListUtilsContactItem] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  public func setItems(_ items: [ShadowListUtilsContactItem], animated: Bool) {
    setSections(shadowListUtilsGroupIntoSections(items), animated: animated)
  }

  public var onPressItem: ((ShadowListUtilsContactItem) -> Void)? {
    didSet {
      allowsSelection = onPressItem != nil
      rebuildRows()
    }
  }

  public var rowForItem: ((ShadowListUtilsContactItem) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsContactsLabels()

  /*
   * The titles of the index along the trailing edge, or nil for none.
   */
  public var sectionIndexTitles: [String]? {
    didSet {
      sectionAdapter?.indexTitles = sectionIndexTitles
      reloadData()
    }
  }

  public var onDelete: ((ShadowListUtilsContactItem) -> Void)?

  public var disclosureIndicator = true {
    didSet {
      if disclosureIndicator != oldValue {
        rebuildRows()
      }
    }
  }

  private var storedSections: [ShadowListUtilsContactSection] = []
  private var storedItems: [ShadowListUtilsContactItem] = []

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    stickySectionHeaders = true
    allowsSelection = false
  }

  override func makeAdapter() -> ShadowListUtilsListAdapter {
    ShadowListUtilsSectionListAdapter()
  }

  private var sectionAdapter: ShadowListUtilsSectionListAdapter? {
    adapter as? ShadowListUtilsSectionListAdapter
  }

  private func makeRow(_ item: ShadowListUtilsContactItem) -> ShadowListUtilsRow {
    rowForItem?(item) ?? ShadowListUtilsContactRow(item: item, showsChevron: onPressItem != nil && disclosureIndicator, theme: theme)
  }

  override func makeRows() -> [ShadowListUtilsRow]? {
    storedItems.map(makeRow)
  }

  override func rebuildRows(animated: Bool = false) {
    showRows(storedItems.map(makeRow), animated: animated)
  }

  /*
   * A new theme or text size changes the headers' size too, which only a reload measures again.
   */
  override func reloadRows() {
    super.reloadRows()
    reloadData()
    refreshHeaders()
  }

  /*
   * Hand the adapter the sections with their header rows, then show the rows of their items.
   */
  private func showRows(_ rows: [ShadowListUtilsRow], animated: Bool) {
    sectionAdapter?.sections = storedSections.map { section in
      let header = ShadowListUtilsSectionHeader(
        key: Self.headerKeyPrefix + section.key, title: section.title, count: section.items.count, theme: theme)
      return ShadowListUtilsListSection(key: section.key, header: header, count: section.items.count)
    }
    setRows(rows, animated: animated)
    refreshHeaders()
  }

  /*
   * A header keeps its cell while its section stays, and the list configures only item cells
   * again. A shown header whose title, count or theme changed takes its new row here.
   */
  private func refreshHeaders() {
    guard let sectionAdapter else { return }
    let headers = Dictionary(sectionAdapter.headerRows.map { ($0.key, $0) }, uniquingKeysWith: { first, _ in first })
    for case let view as ShadowListUtilsSectionHeaderView in subviews where !view.isHidden {
      guard let shown = view.row as? ShadowListUtilsSectionHeader, let next = headers[shown.key] as? ShadowListUtilsSectionHeader,
        next.contentVersion != shown.contentVersion || next.theme != shown.theme
      else { continue }
      sectionAdapter.show(next, in: view)
    }
  }

  override func didSelectItem(at index: Int) {
    if storedItems.indices.contains(index) {
      onPressItem?(storedItems[index])
    }
    deselectItem(at: index, animated: true)
  }

  override func trailingSwipeActions(at index: Int) -> ShadowListKitSwipeActionsConfiguration? {
    guard let onDelete, storedItems.indices.contains(index) else { return nil }
    return .deleting(storedItems[index], title: labels.delete, color: theme.colors.red, onDelete: onDelete)
  }

  override func accessibilityActions(at index: Int) -> [UIAccessibilityCustomAction] {
    guard let onDelete, storedItems.indices.contains(index) else { return [] }
    let item = storedItems[index]
    return [UIAccessibilityCustomAction(name: labels.delete) { _ in
      onDelete(item)
      return true
    }]
  }

  /*
   * Keeps a header row's key apart from the keys of the items, which share the layout cache.
   */
  private static let headerKeyPrefix = "ShadowListUtilsSectionHeader:"
}
