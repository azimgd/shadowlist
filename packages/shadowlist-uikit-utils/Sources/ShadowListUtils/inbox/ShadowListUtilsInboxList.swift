import ShadowListKit
import UIKit

/*
 * A mail list on the kit's own interaction features. Rows swipe: read or unread on the leading
 * side, delete and flag on the trailing side, and a full swipe runs the first action. Touching
 * and holding a row opens a menu. While the list is editing a tap selects rows and swiping and
 * the menu are off. Set the kit's editing to turn it on. Leaving editing clears the selection.
 */
public final class ShadowListUtilsInboxList: ShadowListUtilsList {
  /*
   * The messages. Setting them shows them without an animation.
   */
  public var items: [ShadowListUtilsInboxMessage] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  /*
   * Show these items. animated animates the change from the items before.
   */
  public func setItems(_ items: [ShadowListUtilsInboxMessage], animated: Bool) {
    let rows = rows(for: items, previousItems: storedItems, previousRows: rows, key: \.key, make: makeRow)
    storedItems = items
    setRows(rows, animated: animated)
  }

  /*
   * Runs for a tapped message while the list is not editing.
   */
  public var onPressItem: ((ShadowListUtilsInboxMessage) -> Void)? {
    didSet { updateSelectionMode() }
  }

  /*
   * Replaces the mail row with a row of the app's own for every item.
   */
  public var rowForItem: ((ShadowListUtilsInboxMessage) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsInboxLabels() {
    didSet { reloadRows() }
  }

  /*
   * Run for the swipe actions and the menu entries, with the message as it is in items when the
   * action runs. An action whose closure is nil is left out. Change the item in items in them.
   */
  public var onToggleRead: ((ShadowListUtilsInboxMessage) -> Void)?
  public var onToggleFlag: ((ShadowListUtilsInboxMessage) -> Void)?
  public var onDelete: ((ShadowListUtilsInboxMessage) -> Void)?
  public var onReply: ((ShadowListUtilsInboxMessage) -> Void)?

  /*
   * The keys of the selected messages. Setting it selects those messages without a call to
   * onSelectionChange.
   */
  public var selectedKeys: Set<String> {
    get {
      Set(selectedIndices.compactMap { storedItems.indices.contains($0) ? storedItems[$0].key : nil })
    }
    set {
      for index in selectedIndices {
        deselectItem(at: index, animated: false)
      }
      for (index, item) in storedItems.enumerated() where newValue.contains(item.key) {
        selectItem(at: index, animated: false)
      }
    }
  }

  /*
   * Runs with the selected keys after a tap changed them while editing, with no keys when
   * leaving editing cleared a selection, and with one key for the menu's Select entry, which
   * asks the app to start editing with it.
   */
  public var onSelectionChange: ((Set<String>) -> Void)?

  private var storedItems: [ShadowListUtilsInboxMessage] = []

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    showsSeparators = true
    updateSelectionMode()
  }

  public override func setEditing(_ editing: Bool, animated: Bool) {
    guard editing != isEditing else { return }
    let selected = editing ? IndexSet() : selectedIndices
    super.setEditing(editing, animated: animated)
    for index in selected {
      deselectItem(at: index, animated: false)
    }
    updateSelectionMode()
    rebuildRows()
    if !selected.isEmpty {
      onSelectionChange?([])
    }
  }

  override func makeAdapter() -> ShadowListUtilsListAdapter {
    ShadowListUtilsMenuListAdapter()
  }

  override func applyTheme() {
    super.applyTheme()
    separatorInsetStart = theme.rowInset
    separatorColor = theme.colors.separator
  }

  private func makeRow(_ item: ShadowListUtilsInboxMessage) -> ShadowListUtilsRow {
    rowForItem?(item) ?? ShadowListUtilsInboxRow(item: item, editing: isEditing, theme: theme, labels: labels)
  }

  override func makeRows() -> [ShadowListUtilsRow]? {
    storedItems.map(makeRow)
  }

  /*
   * A tap selects while editing, and otherwise only when the app listens for presses.
   */
  private func updateSelectionMode() {
    allowsMultipleSelection = isEditing
    allowsSelection = isEditing || onPressItem != nil
  }

  // MARK: - Selection

  override func didSelectItem(at index: Int) {
    if isEditing {
      onSelectionChange?(selectedKeys)
      return
    }
    if storedItems.indices.contains(index) {
      onPressItem?(storedItems[index])
    }
    deselectItem(at: index, animated: true)
  }

  override func didDeselectItem(at index: Int) {
    if isEditing {
      onSelectionChange?(selectedKeys)
    }
  }

  // MARK: - Swipe actions

  override func leadingSwipeActions(at index: Int) -> ShadowListKitSwipeActionsConfiguration? {
    guard !isEditing, let onToggleRead, storedItems.indices.contains(index) else { return nil }
    let item = storedItems[index]
    let action = swipeAction(item.read ? labels.unread : labels.read, item.key, onToggleRead)
    action.backgroundColor = theme.colors.blue
    return ShadowListKitSwipeActionsConfiguration(actions: [action])
  }

  override func trailingSwipeActions(at index: Int) -> ShadowListKitSwipeActionsConfiguration? {
    guard !isEditing, storedItems.indices.contains(index) else { return nil }
    let item = storedItems[index]
    var actions: [ShadowListKitSwipeAction] = []
    if let onDelete {
      let action = swipeAction(labels.delete, item.key, onDelete, style: .destructive)
      action.backgroundColor = theme.colors.red
      actions.append(action)
    }
    if let onToggleFlag {
      let action = swipeAction(item.flagged ? labels.unflag : labels.flag, item.key, onToggleFlag)
      action.backgroundColor = theme.colors.orange
      actions.append(action)
    }
    return actions.isEmpty ? nil : ShadowListKitSwipeActionsConfiguration(actions: actions)
  }

  private func swipeAction(
    _ title: String, _ key: String, _ handler: @escaping (ShadowListUtilsInboxMessage) -> Void,
    style: ShadowListKitSwipeActionStyle = .normal
  ) -> ShadowListKitSwipeAction {
    ShadowListKitSwipeAction(style: style, title: title) { [weak self] _, completion in
      completion(self?.perform(handler, key) ?? false)
    }
  }

  /*
   * Run an action's closure with the message as it is now. The message an action was made for
   * may have changed or left the items while its button or menu was open.
   */
  private func perform(_ handler: (ShadowListUtilsInboxMessage) -> Void, _ key: String) -> Bool {
    guard let item = storedItems.first(where: { $0.key == key }) else { return false }
    handler(item)
    return true
  }

  // MARK: - Menu

  /*
   * The entries of a message's menu, in order: reply, read or unread, flag or unflag, select
   * and delete. A screen reader gets the same entries as actions.
   */
  private func menuEntries(at index: Int) -> [(title: String, symbol: String, destructive: Bool, handler: () -> Void)] {
    guard !isEditing, storedItems.indices.contains(index) else { return [] }
    let item = storedItems[index]
    let key = item.key
    var entries: [(title: String, symbol: String, destructive: Bool, handler: () -> Void)] = []
    func add(_ title: String, _ symbol: String, destructive: Bool = false, _ handler: @escaping (ShadowListUtilsInboxMessage) -> Void) {
      entries.append((title, symbol, destructive, { [weak self] in _ = self?.perform(handler, key) }))
    }
    if let onReply {
      add(labels.reply, "arrowshape.turn.up.left", onReply)
    }
    if let onToggleRead {
      add(item.read ? labels.markUnread : labels.markRead, item.read ? "envelope.badge" : "envelope.open", onToggleRead)
    }
    if let onToggleFlag {
      add(item.flagged ? labels.unflag : labels.flag, item.flagged ? "flag.slash" : "flag", onToggleFlag)
    }
    if let onSelectionChange {
      add(labels.select, "checkmark.circle") { onSelectionChange([$0.key]) }
    }
    if let onDelete {
      add(labels.delete, "trash", destructive: true, onDelete)
    }
    return entries
  }

  override func contextMenu(at index: Int) -> UIMenu? {
    let entries = menuEntries(at: index)
    guard !entries.isEmpty else { return nil }
    let actions = entries.map { entry in
      UIAction(title: entry.title, image: UIImage(systemName: entry.symbol), attributes: entry.destructive ? .destructive : []) { _ in
        entry.handler()
      }
    }
    return UIMenu(title: storedItems[index].subject, children: actions)
  }

  override func accessibilityActions(at index: Int) -> [UIAccessibilityCustomAction] {
    menuEntries(at: index).map { entry in
      UIAccessibilityCustomAction(name: entry.title) { _ in
        entry.handler()
        return true
      }
    }
  }
}
