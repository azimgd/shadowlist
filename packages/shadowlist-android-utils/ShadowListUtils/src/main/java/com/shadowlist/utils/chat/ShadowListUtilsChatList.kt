package com.shadowlist.utils

import android.content.Context
import android.view.HapticFeedbackConstants

/*
 * A chat thread: an inverted list that opens at its end and keeps what is on screen in place
 * while messages arrive. Items are oldest first.
 */
class ShadowListUtilsChatList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  /*
   * The messages. Setting them shows them without an animation.
   */
  var items: List<ShadowListUtilsChatMessage>
    get() = storedItems
    set(value) = setItems(value, false)

  /*
   * Show these items. animated animates the change from the items before.
   */
  fun setItems(items: List<ShadowListUtilsChatMessage>, animated: Boolean) {
    val next = rows(items, storedItems, rows, { it.key }, ::makeRow)
    storedItems = items
    setRows(next, animated)
  }

  /*
   * Runs for a tapped message. Without it the messages do not select.
   */
  var onPressItem: ((ShadowListUtilsChatMessage) -> Unit)? = null
    set(value) {
      field = value
      allowsSelection = value != null
    }

  /*
   * Replaces the bubble with a row of the app's own for every item.
   */
  var rowForItem: ((ShadowListUtilsChatMessage) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels: ShadowListUtilsChatLabels = ShadowListUtilsChatLabels()
    set(value) {
      field = value
      reloadRows()
    }

  /*
   * Runs for a tap on the line under a failed message.
   */
  var onRetryMessage: ((ShadowListUtilsChatMessage) -> Unit)? = null
    set(value) {
      val changed = (field == null) != (value == null)
      field = value
      // The mounted rows turn their failed line into a button, or back.
      if (changed) reloadRows()
    }

  /*
   * Runs for a hold on a message's bubble. The lift that ends the hold is not a tap.
   */
  var onLongPressMessage: ((ShadowListUtilsChatMessage) -> Unit)? = null

  /*
   * Gives a message the small line inside its bubble, like its time, or null for none.
   */
  var captionForItem: ((ShadowListUtilsChatMessage) -> String?)? = null
    set(value) {
      field = value
      reloadRows()
    }

  /*
   * Whether messages from others show their sender's avatar and name.
   */
  var showsAuthor: Boolean = true
    set(value) {
      if (field == value) return
      field = value
      rebuildRows()
    }

  private var storedItems: List<ShadowListUtilsChatMessage> = emptyList()

  init {
    inverted = true
    allowsSelection = false
  }

  private fun makeRow(item: ShadowListUtilsChatMessage): ShadowListUtilsRow =
    rowForItem?.invoke(item) ?: ShadowListUtilsChatBubble(item, captionForItem?.invoke(item), showsAuthor, theme, labels)

  override fun makeRows(): List<ShadowListUtilsRow> = storedItems.map(::makeRow)

  override fun didSelectItem(index: Int) {
    storedItems.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }

  /*
   * The kit tells of a hold on an item, and its lift selects nothing. A hold counts when it is
   * on the message's bubble. It goes to onLongPressMessage with the system's tick.
   */
  override fun didLongPressItem(index: Int): Boolean {
    val onLongPressMessage = onLongPressMessage ?: return false
    val item = storedItems.getOrNull(index) ?: return false
    val view = cellForItem(index) as? ShadowListUtilsChatBubbleView ?: return false
    if (!view.bubbleContains(touchXIn(view), touchYIn(view))) return false
    view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    onLongPressMessage(item)
    return true
  }

  /*
   * A screen reader retries a failed message through an action.
   */
  override fun accessibilityActions(index: Int): List<ShadowListUtilsAccessibilityAction> {
    val onRetryMessage = onRetryMessage ?: return emptyList()
    val item = storedItems.getOrNull(index)?.takeIf { it.status == ShadowListUtilsChatMessageStatus.FAILED } ?: return emptyList()
    return listOf(ShadowListUtilsAccessibilityAction(R.id.shadowlist_utils_action_retry, labels.retryHint) {
      onRetryMessage(item)
      true
    })
  }
}
