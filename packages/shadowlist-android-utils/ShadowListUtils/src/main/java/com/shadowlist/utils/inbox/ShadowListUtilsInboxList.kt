package com.shadowlist.utils

import android.content.Context
import android.view.Menu
import com.shadowlist.kit.ShadowListKitSwipeAction
import com.shadowlist.kit.ShadowListKitSwipeActionsConfiguration

/*
 * A mail list on the kit's own interaction features. Rows swipe: read or unread on the leading
 * side, delete and flag on the trailing side, and a full swipe runs the first action. Touching
 * and holding a row opens a menu. While the list is editing a tap selects rows and swiping and
 * the menu are off. Set the kit's editing to turn it on. Leaving editing clears the selection.
 */
class ShadowListUtilsInboxList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  /*
   * The messages. Setting them shows them without an animation.
   */
  var items: List<ShadowListUtilsInboxMessage>
    get() = storedItems
    set(value) = setItems(value, false)

  /*
   * Show these items. animated animates the change from the items before.
   */
  fun setItems(items: List<ShadowListUtilsInboxMessage>, animated: Boolean) {
    val next = rows(items, storedItems, rows, { it.key }, ::makeRow)
    storedItems = items
    setRows(next, animated)
  }

  /*
   * Runs for a tapped message while the list is not editing.
   */
  var onPressItem: ((ShadowListUtilsInboxMessage) -> Unit)? = null
    set(value) {
      field = value
      updateSelectionMode()
    }

  /*
   * Replaces the mail row with a row of the app's own for every item.
   */
  var rowForItem: ((ShadowListUtilsInboxMessage) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels: ShadowListUtilsInboxLabels = ShadowListUtilsInboxLabels()
    set(value) {
      field = value
      reloadRows()
    }

  /*
   * Run for the swipe actions and the menu entries, with the message as it is in items when the
   * action runs. An action whose closure is null is left out. Change the item in items in them.
   */
  var onToggleRead: ((ShadowListUtilsInboxMessage) -> Unit)? = null
  var onToggleFlag: ((ShadowListUtilsInboxMessage) -> Unit)? = null
  var onDelete: ((ShadowListUtilsInboxMessage) -> Unit)? = null
  var onReply: ((ShadowListUtilsInboxMessage) -> Unit)? = null

  /*
   * The keys of the selected messages. Setting it selects those messages without a call to
   * onSelectionChange.
   */
  var selectedKeys: Set<String>
    get() {
      val keys = LinkedHashSet<String>()
      for (index in selectedIndices) storedItems.getOrNull(index)?.let { keys.add(it.key) }
      return keys
    }
    set(value) {
      for (index in selectedIndices) deselectItem(index)
      for ((index, item) in storedItems.withIndex()) if (item.key in value) selectItem(index)
    }

  /*
   * Runs with the selected keys after a tap changed them while editing, with no keys when
   * leaving editing cleared a selection, and with one key for the menu's Select entry, which
   * asks the app to start editing with it.
   */
  var onSelectionChange: ((Set<String>) -> Unit)? = null

  private var storedItems: List<ShadowListUtilsInboxMessage> = emptyList()

  init {
    showsSeparators = true
    applyTheme()
    updateSelectionMode()
  }

  override fun setEditing(editing: Boolean, animated: Boolean) {
    if (editing == this.editing) return
    val selected = if (editing) IntArray(0) else selectedIndices
    super.setEditing(editing, animated)
    for (index in selected) deselectItem(index)
    updateSelectionMode()
    rebuildRows()
    if (selected.isNotEmpty()) onSelectionChange?.invoke(emptySet())
  }

  override fun applyTheme() {
    super.applyTheme()
    separatorInsetStartPx = theme.rowInset.px
    separatorColor = theme.colors.separator
  }

  private fun makeRow(item: ShadowListUtilsInboxMessage): ShadowListUtilsRow =
    rowForItem?.invoke(item) ?: ShadowListUtilsInboxRow(item, editing, theme, labels)

  override fun makeRows(): List<ShadowListUtilsRow> = storedItems.map(::makeRow)

  /*
   * A tap selects while editing, and otherwise only when the app listens for presses.
   */
  private fun updateSelectionMode() {
    allowsMultipleSelection = editing
    allowsSelection = editing || onPressItem != null
  }

  // region Selection

  override fun didSelectItem(index: Int) {
    if (editing) {
      onSelectionChange?.invoke(selectedKeys)
      return
    }
    storedItems.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }

  override fun didDeselectItem(index: Int) {
    if (editing) onSelectionChange?.invoke(selectedKeys)
  }

  // endregion

  // region Swipe actions

  override fun leadingSwipeActions(index: Int): ShadowListKitSwipeActionsConfiguration? {
    val onToggleRead = onToggleRead ?: return null
    val item = storedItems.getOrNull(index) ?: return null
    if (editing) return null
    val action = swipeAction(if (item.read) labels.unread else labels.read, item.key, onToggleRead)
    action.backgroundColor = theme.colors.blue
    return ShadowListKitSwipeActionsConfiguration(listOf(action))
  }

  override fun trailingSwipeActions(index: Int): ShadowListKitSwipeActionsConfiguration? {
    val item = storedItems.getOrNull(index) ?: return null
    if (editing) return null
    val actions = ArrayList<ShadowListKitSwipeAction>()
    onDelete?.let {
      val action = swipeAction(labels.delete, item.key, it, ShadowListKitSwipeAction.Style.DESTRUCTIVE)
      action.backgroundColor = theme.colors.red
      actions.add(action)
    }
    onToggleFlag?.let {
      val action = swipeAction(if (item.flagged) labels.unflag else labels.flag, item.key, it)
      action.backgroundColor = theme.colors.orange
      actions.add(action)
    }
    return if (actions.isEmpty()) null else ShadowListKitSwipeActionsConfiguration(actions)
  }

  private fun swipeAction(
    title: String,
    key: String,
    handler: (ShadowListUtilsInboxMessage) -> Unit,
    style: ShadowListKitSwipeAction.Style = ShadowListKitSwipeAction.Style.NORMAL,
  ) = ShadowListKitSwipeAction(style, title) { _, completion -> completion(perform(handler, key)) }

  /*
   * Run an action's closure with the message as it is now. The message an action was made for
   * may have changed or left the items while its button or menu was open.
   */
  private fun perform(handler: (ShadowListUtilsInboxMessage) -> Unit, key: String): Boolean {
    val item = storedItems.firstOrNull { it.key == key } ?: return false
    handler(item)
    return true
  }

  // endregion

  // region Menu

  private class MenuEntry(val id: Int, val title: String, val handler: () -> Boolean)

  /*
   * The entries of a message's menu, in order: reply, read or unread, flag or unflag, select
   * and delete. A screen reader gets the same entries as actions.
   */
  private fun menuEntries(index: Int): List<MenuEntry> {
    val item = storedItems.getOrNull(index) ?: return emptyList()
    if (editing) return emptyList()
    val key = item.key
    val entries = ArrayList<MenuEntry>()
    fun add(id: Int, title: String, handler: (ShadowListUtilsInboxMessage) -> Unit) {
      entries.add(MenuEntry(id, title) { perform(handler, key) })
    }
    onReply?.let { add(R.id.shadowlist_utils_action_reply, labels.reply, it) }
    onToggleRead?.let {
      add(R.id.shadowlist_utils_action_toggle_read, if (item.read) labels.markUnread else labels.markRead, it)
    }
    onToggleFlag?.let { add(R.id.shadowlist_utils_action_toggle_flag, if (item.flagged) labels.unflag else labels.flag, it) }
    onSelectionChange?.let { report -> add(R.id.shadowlist_utils_action_select, labels.select) { report(setOf(it.key)) } }
    onDelete?.let { add(R.id.shadowlist_utils_action_delete, labels.delete, it) }
    return entries
  }

  /*
   * The platform's menu has no title. The subject is a first entry that cannot be chosen.
   */
  override fun contextMenu(index: Int, menu: Menu): Boolean {
    val entries = menuEntries(index)
    if (entries.isEmpty()) return false
    menu.add(storedItems[index].subject).isEnabled = false
    for (entry in entries) {
      menu.add(entry.title).setOnMenuItemClickListener {
        entry.handler()
        true
      }
    }
    return true
  }

  override fun accessibilityActions(index: Int): List<ShadowListUtilsAccessibilityAction> =
    menuEntries(index).map { ShadowListUtilsAccessibilityAction(it.id, it.title, it.handler) }

  // endregion
}
