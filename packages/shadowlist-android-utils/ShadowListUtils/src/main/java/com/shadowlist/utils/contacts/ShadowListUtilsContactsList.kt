package com.shadowlist.utils

import android.content.Context
import com.shadowlist.kit.ShadowListKitSwipeAction
import com.shadowlist.kit.ShadowListKitSwipeActionsConfiguration

/*
 * A list of contacts.
 */
class ShadowListUtilsContactsList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  /*
   * The contacts. Setting them shows them without an animation.
   */
  var items: List<ShadowListUtilsContactItem>
    get() = storedItems
    set(value) = setItems(value, false)

  /*
   * Show these items. animated animates the change from the items before.
   */
  fun setItems(items: List<ShadowListUtilsContactItem>, animated: Boolean) {
    val next = rows(items, storedItems, rows, { it.key }, ::makeRow)
    storedItems = items
    setRows(next, animated)
  }

  /*
   * Runs for a tapped contact. Without it the contacts do not select.
   */
  var onPressItem: ((ShadowListUtilsContactItem) -> Unit)? = null
    set(value) {
      field = value
      allowsSelection = value != null
      rebuildRows()
    }

  /*
   * Replaces the contact row with a row of the app's own for every item.
   */
  var rowForItem: ((ShadowListUtilsContactItem) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels: ShadowListUtilsContactsLabels = ShadowListUtilsContactsLabels()

  /*
   * Adds a trailing swipe action that deletes a contact. Delete the item from items in it.
   */
  var onDelete: ((ShadowListUtilsContactItem) -> Unit)? = null

  /*
   * Shows a chevron on every row while onPressItem is set.
   */
  var disclosureIndicator: Boolean = true
    set(value) {
      if (field == value) return
      field = value
      rebuildRows()
    }

  private var storedItems: List<ShadowListUtilsContactItem> = emptyList()

  init {
    allowsSelection = false
  }

  private fun makeRow(item: ShadowListUtilsContactItem): ShadowListUtilsRow =
    rowForItem?.invoke(item) ?: ShadowListUtilsContactRow(item, onPressItem != null && disclosureIndicator, theme)

  override fun makeRows(): List<ShadowListUtilsRow> = storedItems.map(::makeRow)

  override fun didSelectItem(index: Int) {
    storedItems.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }

  override fun trailingSwipeActions(index: Int): ShadowListKitSwipeActionsConfiguration? {
    val onDelete = onDelete ?: return null
    val item = storedItems.getOrNull(index) ?: return null
    return deletingSwipeActions(item, labels.delete, theme.colors.red, onDelete)
  }

  override fun accessibilityActions(index: Int): List<ShadowListUtilsAccessibilityAction> {
    val onDelete = onDelete ?: return emptyList()
    val item = storedItems.getOrNull(index) ?: return emptyList()
    return listOf(ShadowListUtilsAccessibilityAction(R.id.shadowlist_utils_action_delete, labels.delete) {
      onDelete(item)
      true
    })
  }
}

/*
 * One destructive action in the theme's red that hands the contact to onDelete.
 */
internal fun deletingSwipeActions(
  item: ShadowListUtilsContactItem,
  title: String,
  color: Int,
  onDelete: (ShadowListUtilsContactItem) -> Unit,
): ShadowListKitSwipeActionsConfiguration {
  val action = ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.DESTRUCTIVE, title) { _, completion ->
    onDelete(item)
    completion(true)
  }
  action.backgroundColor = color
  return ShadowListKitSwipeActionsConfiguration(listOf(action))
}
