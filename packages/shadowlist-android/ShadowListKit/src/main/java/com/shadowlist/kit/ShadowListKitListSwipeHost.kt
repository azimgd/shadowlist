package com.shadowlist.kit

import android.view.View
import android.view.ViewGroup

/* The list keeps its item identity, selection and gesture policy when sharing swipe reveal. */
internal class ShadowListKitListSwipeHost(private val list: ShadowListKitListView) : ShadowListKitSwipeHost {
  override val view: ViewGroup get() = list
  override val horizontal: Boolean get() = list.horizontal
  override val density: Float get() = list.density
  override val isScrolling: Boolean get() = list.gesture.isTracking
  override val allowsSwipe: Boolean get() = !list.editing && !list.hasHeldRow

  override fun itemCellAt(x: Float, y: Float): ShadowListKitListCell? = list.itemCellAt(x, y)

  override fun actions(cell: ShadowListKitListCell, leading: Boolean): ShadowListKitSwipeActionsConfiguration? {
    val delegate = list.delegate ?: return null
    return if (leading) delegate.leadingSwipeActionsForItem(list, cell.index)
      else delegate.trailingSwipeActionsForItem(list, cell.index)
  }

  override fun isValid(cell: ShadowListKitListCell): Boolean = list.isCurrentActionCell(cell)

  override fun keyForCell(cell: ShadowListKitListCell): String? = list.keys.getOrNull(cell.row)

  override fun beginSwipe() {
    list.abandonScrollGesture()
    list.cancelChildTouches()
  }

  override fun cancelHighlight() = list.selection.cancelHighlight()
  override fun addActionsView(view: View) = list.addSwipeActionsView(view)
  override fun removeActionsView(view: View) = list.removeSwipeActionsView(view)
}
