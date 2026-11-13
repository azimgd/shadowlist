package com.shadowlist.utils

import android.content.Context

/*
 * A feed of posts.
 */
class ShadowListUtilsFeedList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  /*
   * The posts. Setting them shows them without an animation.
   */
  var items: List<ShadowListUtilsFeedItem>
    get() = storedItems
    set(value) = setItems(value, false)

  /*
   * Show these items. animated animates the change from the items before.
   */
  fun setItems(items: List<ShadowListUtilsFeedItem>, animated: Boolean) {
    val next = rows(items, storedItems, rows, { it.key }, ::makeRow)
    storedItems = items
    setRows(next, animated)
  }

  /*
   * Runs for a tapped post. Without it the posts do not select.
   */
  var onPressItem: ((ShadowListUtilsFeedItem) -> Unit)? = null
    set(value) {
      field = value
      allowsSelection = value != null
    }

  /*
   * Replaces the post row with a row of the app's own for every item.
   */
  var rowForItem: ((ShadowListUtilsFeedItem) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels: ShadowListUtilsFeedLabels = ShadowListUtilsFeedLabels()
    set(value) {
      field = value
      reloadRows()
    }

  /*
   * Runs for a tapped image with its post and its position among the post's images. While it
   * is set a screen reader reads every image as a button.
   */
  var onPressImage: ((ShadowListUtilsFeedItem, Int) -> Unit)? = null
    set(value) {
      val changed = (field == null) != (value == null)
      field = value
      // The mounted rows turn their images into buttons, or back.
      if (changed) reloadRows()
    }

  /*
   * Writes a post's age from its createdAtMs in place of the labels' words.
   */
  var formatTime: ((Double) -> String)? = null
    set(value) {
      field = value
      reloadRows()
    }

  private var storedItems: List<ShadowListUtilsFeedItem> = emptyList()

  init {
    allowsSelection = false
  }

  private fun makeRow(item: ShadowListUtilsFeedItem): ShadowListUtilsRow =
    rowForItem?.invoke(item) ?: ShadowListUtilsFeedRow(item, theme, labels, formatTime)

  override fun makeRows(): List<ShadowListUtilsRow> = storedItems.map(::makeRow)

  override fun didSelectItem(index: Int) {
    storedItems.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }
}
