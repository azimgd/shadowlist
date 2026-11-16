package com.shadowlist.utils

import android.content.Context

/*
 * Activities in the app's order. Presses are controlled by the app; the list never marks an
 * activity read itself. Pull to refresh and edge callbacks come from ShadowListUtilsList.
 */
class ShadowListUtilsActivityList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  var items: List<ShadowListUtilsActivityItem>
    get() = storedItems
    set(value) = setItems(value, false)

  fun setItems(items: List<ShadowListUtilsActivityItem>, animated: Boolean) {
    val next = rows(items, storedItems, rows, { it.key }, ::makeRow)
    storedItems = items
    setRows(next, animated)
  }

  var onPressItem: ((ShadowListUtilsActivityItem) -> Unit)? = null
    set(value) {
      field = value
      allowsSelection = value != null
      rebuildRows()
    }

  var rowForItem: ((ShadowListUtilsActivityItem) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels = ShadowListUtilsActivityLabels()
    set(value) {
      field = value
      reloadRows()
    }

  var formatTime: ((Double) -> String)? = null
    set(value) {
      field = value
      reloadRows()
    }

  private var storedItems: List<ShadowListUtilsActivityItem> = emptyList()

  init {
    allowsSelection = false
    showsSeparators = true
    separatorInsetStartPx = theme.rowInset.px
    separatorColor = theme.colors.separator
  }

  private fun makeRow(item: ShadowListUtilsActivityItem): ShadowListUtilsRow =
    rowForItem?.invoke(item) ?: ShadowListUtilsActivityRow(item, onPressItem != null, theme, labels, formatTime)

  override fun makeRows(): List<ShadowListUtilsRow> = storedItems.map(::makeRow)

  override fun didSelectItem(index: Int) {
    storedItems.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }
}
