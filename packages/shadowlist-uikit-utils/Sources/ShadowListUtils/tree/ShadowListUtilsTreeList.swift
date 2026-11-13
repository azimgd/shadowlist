import ShadowListKit
import UIKit

/*
 * A file tree whose visible rows and expansion splices come from the shared TreeSource.
 * expandedKeys nil owns expansion here; a set makes expansion controlled by the app.
 * A controlled press only requests onExpandedChange until the app supplies its next set.
 */
public final class ShadowListUtilsTreeList: ShadowListUtilsList {
  public var items: [ShadowListUtilsTreeNode] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  public func setItems(_ items: [ShadowListUtilsTreeNode], animated: Bool) {
    pendingExpandedKeys = nil
    let expansion = expandedKeys ?? ownExpandedKeys
    storedItems = items
    nodes.removeAll(keepingCapacity: true)
    var encoded: [ShadowListKitTreeRow] = []
    func encode(_ items: [ShadowListUtilsTreeNode], depth: Int) {
      for item in items {
        precondition(nodes[item.key] == nil, "Tree keys must be unique")
        nodes[item.key] = item
        encoded.append(ShadowListKitTreeRow(key: item.key, depth: UInt(depth), hasChildren: !(item.children?.isEmpty ?? true), expanded: expansion.contains(item.key)))
        if let children = item.children { encode(children, depth: depth + 1) }
      }
    }
    encode(items, depth: 0)
    source.reset(encoded)
    source.setExpandedKeys(Array(expansion))
    rebuildRows(animated: animated)
  }

  public var expandedKeys: Set<String>? {
    didSet {
      pendingExpandedKeys = nil
      source.setExpandedKeys(Array(expandedKeys ?? ownExpandedKeys))
      rebuildRows()
    }
  }
  public var onExpandedChange: ((Set<String>) -> Void)?
  public var onPressItem: ((ShadowListUtilsTreeNode) -> Void)? {
    didSet { rebuildRows() }
  }
  public var rowForItem: ((ShadowListUtilsTreeListRenderItemInfo) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }
  public var indentWidth: CGFloat = 16 {
    didSet { reloadRows() }
  }
  public var labels = ShadowListUtilsTreeLabels() {
    didSet { reloadRows() }
  }

  private var storedItems: [ShadowListUtilsTreeNode] = []
  private var nodes: [String: ShadowListUtilsTreeNode] = [:]
  private let source = ShadowListKitTreeRows()

  public init(theme: ShadowListUtilsTheme = .system, initialExpandedKeys: Set<String> = []) {
    super.init(theme: theme)
    ownExpandedKeys = initialExpandedKeys
    source.setExpandedKeys(Array(initialExpandedKeys))
    allowsSelection = true
  }

  private var ownExpandedKeys: Set<String> = []
  private var pendingExpandedKeys: Set<String>?

  override func makeRows() -> [ShadowListUtilsRow]? {
    return source.rows.compactMap { row in
      guard let item = nodes[row.key] else { return nil }
      let info = ShadowListUtilsTreeListRenderItemInfo(
        item: item, depth: Int(row.depth), indent: CGFloat(row.depth) * max(0, indentWidth), isExpanded: row.isExpanded,
        hasChildren: row.hasChildren, toggle: { [weak self] in self?.toggle(row.key) })
      return rowForItem?(info)
        ?? ShadowListUtilsTreeRow(item: item, depth: Int(row.depth), hasChildren: row.hasChildren, isExpanded: row.isExpanded,
                                 indentWidth: indentWidth, interactive: row.hasChildren || onPressItem != nil, theme: theme, labels: labels)
    }
  }

  public func scrollToNode(_ key: String, viewPosition: CGFloat = 0) {
    guard let index = source.rows.firstIndex(where: { $0.key == key }) else { return }
    scrollToItem(at: index, viewPosition: viewPosition, animated: true)
  }

  public func toggle(_ key: String) {
    let before = Set(source.expandedKeys)
    guard let index = source.rows.firstIndex(where: { $0.key == key }) else { return }
    let row = source.rows[index]
    guard row.hasChildren else { return }
    if let pendingExpandedKeys { source.setExpandedKeys(Array(pendingExpandedKeys)) }
    source.toggleKey(key)
    let requested = Set(source.expandedKeys)
    if expandedKeys != nil {
      pendingExpandedKeys = requested
      source.setExpandedKeys(Array(before))
    } else {
      ownExpandedKeys = requested
      rebuildRows()
    }
    onExpandedChange?(requested)
  }

  override func didSelectItem(at index: Int) {
    guard source.rows.indices.contains(index) else { return }
    let row = source.rows[index]
    if row.hasChildren { toggle(row.key) } else if let node = nodes[row.key] { onPressItem?(node) }
    deselectItem(at: index, animated: false)
  }
}
