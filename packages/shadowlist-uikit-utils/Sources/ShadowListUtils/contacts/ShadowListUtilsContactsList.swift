import ShadowListKit
import UIKit

/*
 * A list of contacts.
 */
public final class ShadowListUtilsContactsList: ShadowListUtilsList {
  /*
   * The contacts. Setting them shows them without an animation.
   */
  public var items: [ShadowListUtilsContactItem] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  /*
   * Show these items. animated animates the change from the items before.
   */
  public func setItems(_ items: [ShadowListUtilsContactItem], animated: Bool) {
    let rows = rows(for: items, previousItems: storedItems, previousRows: rows, key: \.key, make: makeRow)
    storedItems = items
    setRows(rows, animated: animated)
  }

  /*
   * Runs for a tapped contact. Without it the contacts do not select.
   */
  public var onPressItem: ((ShadowListUtilsContactItem) -> Void)? {
    didSet {
      allowsSelection = onPressItem != nil
      rebuildRows()
    }
  }

  /*
   * Replaces the contact row with a row of the app's own for every item.
   */
  public var rowForItem: ((ShadowListUtilsContactItem) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsContactsLabels()

  /*
   * Adds a trailing swipe action that deletes a contact. Delete the item from items in it.
   */
  public var onDelete: ((ShadowListUtilsContactItem) -> Void)?

  /*
   * Shows a chevron on every row while onPressItem is set.
   */
  public var disclosureIndicator = true {
    didSet {
      if disclosureIndicator != oldValue {
        rebuildRows()
      }
    }
  }

  private var storedItems: [ShadowListUtilsContactItem] = []

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    allowsSelection = false
  }

  private func makeRow(_ item: ShadowListUtilsContactItem) -> ShadowListUtilsRow {
    rowForItem?(item) ?? ShadowListUtilsContactRow(item: item, showsChevron: onPressItem != nil && disclosureIndicator, theme: theme)
  }

  override func makeRows() -> [ShadowListUtilsRow]? {
    storedItems.map(makeRow)
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
}

extension ShadowListKitSwipeActionsConfiguration {
  /*
   * One destructive action in the theme's red that hands the contact to onDelete.
   */
  static func deleting(
    _ item: ShadowListUtilsContactItem, title: String, color: ShadowListUtilsColor,
    onDelete: @escaping (ShadowListUtilsContactItem) -> Void
  ) -> ShadowListKitSwipeActionsConfiguration {
    let action = ShadowListKitSwipeAction(style: .destructive, title: title) { _, completion in
      onDelete(item)
      completion(true)
    }
    action.backgroundColor = color
    return ShadowListKitSwipeActionsConfiguration(actions: [action])
  }
}
