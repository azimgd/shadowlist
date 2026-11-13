package com.shadowlist.utils

import android.content.Context

/*
 * A list of tall cards whose scroll position rests on a card's leading edge.
 */
class ShadowListUtilsSnapList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  /*
   * The cards. Setting them shows them without an animation.
   */
  var items: List<ShadowListUtilsSnapItem>
    get() = storedItems
    set(value) = setItems(value, false)

  /*
   * Show these items. animated animates the change from the items before.
   */
  fun setItems(items: List<ShadowListUtilsSnapItem>, animated: Boolean) {
    val next = rows(items, storedItems, rows, { it.key }, ::makeRow)
    storedItems = items
    setRows(next, animated)
  }

  /*
   * Runs for a tapped card. Without it the cards do not select.
   */
  var onPressItem: ((ShadowListUtilsSnapItem) -> Unit)? = null
    set(value) {
      field = value
      allowsSelection = value != null
    }

  /*
   * Replaces the card with a row of the app's own for every item.
   */
  var rowForItem: ((ShadowListUtilsSnapItem) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels: ShadowListUtilsSnapLabels = ShadowListUtilsSnapLabels()
    set(value) {
      field = value
      reloadRows()
    }

  /*
   * The height of a card with its margin, in dp. Null means a quarter of the height of the
   * window the list is in.
   */
  var cardHeight: Float? = null
    set(value) {
      if (field == value) return
      field = value
      rebuildRows()
    }

  private var storedItems: List<ShadowListUtilsSnapItem> = emptyList()

  /*
   * The height of the window the cards without a height were last sized for.
   */
  private var windowHeightPx = ShadowListUtilsPlatform.windowHeightPx

  init {
    snapToItem = true
    allowsSelection = false
  }

  /*
   * A window of another height gives the cards without a height of their own a new one.
   */
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val height = if (isAttachedToWindow) rootView.height else 0
    if (height > 0 && height != windowHeightPx) {
      windowHeightPx = height
      ShadowListUtilsPlatform.windowHeightPx = height
      if (cardHeight == null) rebuildRows()
    }
    super.onMeasure(widthMeasureSpec, heightMeasureSpec)
  }

  private fun makeRow(item: ShadowListUtilsSnapItem): ShadowListUtilsRow {
    val height = cardHeight ?: ShadowListUtilsSnapCard.defaultHeightPx(windowHeightPx).toFloat().dp
    return rowForItem?.invoke(item) ?: ShadowListUtilsSnapCard(item, height, theme, labels)
  }

  override fun makeRows(): List<ShadowListUtilsRow> = storedItems.map(::makeRow)

  override fun didSelectItem(index: Int) {
    storedItems.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }
}
