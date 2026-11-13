package com.shadowlist.utils

import android.content.Context

/*
 * A grid of image cards in three columns. Each card keeps its image's aspect, and the columns
 * stack their own cards.
 */
class ShadowListUtilsMasonryList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  /*
   * The cards. Setting them shows them without an animation.
   */
  var items: List<ShadowListUtilsMasonryItem>
    get() = storedItems
    set(value) = setItems(value, false)

  /*
   * Show these items. animated animates the change from the items before.
   */
  fun setItems(items: List<ShadowListUtilsMasonryItem>, animated: Boolean) {
    val next = rows(items, storedItems, rows, { it.key }, ::makeRow)
    storedItems = items
    setRows(next, animated)
  }

  /*
   * Runs for a tapped card. Without it the cards do not select.
   */
  var onPressItem: ((ShadowListUtilsMasonryItem) -> Unit)? = null
    set(value) {
      field = value
      allowsSelection = value != null
    }

  /*
   * Replaces the card with a row of the app's own for every item.
   */
  var rowForItem: ((ShadowListUtilsMasonryItem) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels: ShadowListUtilsMasonryLabels = ShadowListUtilsMasonryLabels()
    set(value) {
      field = value
      reloadRows()
    }

  private var storedItems: List<ShadowListUtilsMasonryItem> = emptyList()

  init {
    numberOfColumns = COLUMNS
    allowsSelection = false
  }

  private fun makeRow(item: ShadowListUtilsMasonryItem): ShadowListUtilsRow =
    rowForItem?.invoke(item) ?: ShadowListUtilsMasonryCard(item, theme, labels)

  override fun makeRows(): List<ShadowListUtilsRow> = storedItems.map(::makeRow)

  override fun didSelectItem(index: Int) {
    storedItems.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }

  private companion object {
    const val COLUMNS = 3
  }
}
