package com.shadowlist.utils

import android.content.Context

/*
 * A grid of tiles in three columns the reader drags into a new order: touch and hold a tile,
 * then drag it.
 */
class ShadowListUtilsReorderGrid : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  /*
   * The tiles. Setting them shows them without an animation.
   */
  var items: List<ShadowListUtilsReorderTileItem>
    get() = storedItems
    set(value) = setItems(value, false)

  /*
   * Show these items. animated animates the change from the items before.
   */
  fun setItems(items: List<ShadowListUtilsReorderTileItem>, animated: Boolean) {
    val next = rows(items, storedItems, rows, { it.key }, ::makeRow)
    storedItems = items
    setRows(next, animated)
  }

  /*
   * Runs for a tapped tile. Without it the tiles do not select.
   */
  var onPressItem: ((ShadowListUtilsReorderTileItem) -> Unit)? = null
    set(value) {
      field = value
      allowsSelection = value != null
    }

  /*
   * Replaces the tile with a row of the app's own for every item.
   */
  var rowForItem: ((ShadowListUtilsReorderTileItem) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels: ShadowListUtilsReorderLabels = ShadowListUtilsReorderLabels()
    set(value) {
      field = value
      reloadRows()
    }

  /*
   * Runs after a tile was dropped at a new place, or moved by one place through a screen
   * reader action, with the source index, the destination index and the items after the move.
   * items already holds that order. A tile dropped where it was picked up moved nowhere and
   * does not run it.
   */
  var onMoveItem: ((Int, Int, List<ShadowListUtilsReorderTileItem>) -> Unit)? = null

  /*
   * Gives every swatch the same width over height, in place of each item's own.
   */
  var tileAspectRatio: Float? = null
    set(value) {
      if (field == value) return
      field = value
      rebuildRows()
    }

  private var storedItems: List<ShadowListUtilsReorderTileItem> = emptyList()

  init {
    reorderEnabled = true
    numberOfColumns = COLUMNS
    allowsSelection = false
  }

  private fun makeRow(item: ShadowListUtilsReorderTileItem): ShadowListUtilsRow =
    rowForItem?.invoke(item) ?: ShadowListUtilsReorderTile(item, tileAspectRatio, theme, labels)

  override fun makeRows(): List<ShadowListUtilsRow> = storedItems.map(::makeRow)

  override fun didSelectItem(index: Int) {
    storedItems.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }

  /*
   * The kit dropped a tile. It reads the data again right after, which is already in the new
   * order here.
   */
  override fun didMoveItem(sourceIndex: Int, destinationIndex: Int) {
    if (sourceIndex == destinationIndex) return
    storedItems = ShadowListUtilsArrayMove.moved(storedItems, sourceIndex, destinationIndex)
    super.didMoveItem(sourceIndex, destinationIndex)
    onMoveItem?.invoke(sourceIndex, destinationIndex, storedItems)
  }

  override fun accessibilityActions(index: Int): List<ShadowListUtilsAccessibilityAction> {
    if (onMoveItem == null) return emptyList()
    return listOf(
      ShadowListUtilsAccessibilityAction(R.id.shadowlist_utils_action_move_backward, labels.moveEarlier) { moveItemBy(index, -1) },
      ShadowListUtilsAccessibilityAction(R.id.shadowlist_utils_action_move_forward, labels.moveLater) { moveItemBy(index, 1) })
  }

  /*
   * Move an item by one place for a screen reader, the same path as a drop.
   */
  private fun moveItemBy(index: Int, offset: Int): Boolean {
    val destinationIndex = index + offset
    if (index !in storedItems.indices || destinationIndex !in storedItems.indices) return false
    setItems(ShadowListUtilsArrayMove.moved(storedItems, index, destinationIndex), true)
    onMoveItem?.invoke(index, destinationIndex, storedItems)
    return true
  }

  private companion object {
    const val COLUMNS = 3
  }
}
