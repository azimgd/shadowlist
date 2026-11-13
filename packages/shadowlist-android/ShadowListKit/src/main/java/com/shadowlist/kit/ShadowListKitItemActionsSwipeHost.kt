package com.shadowlist.kit

import android.view.View
import android.view.ViewGroup

/* Standalone content has no list index; attachment identifies its current native surface. */
internal class ShadowListKitItemActionsSwipeHost(private val actionsView: ShadowListKitItemActionsView) : ShadowListKitSwipeHost {
  override val view: ViewGroup get() = actionsView
  override val horizontal = false
  override val density: Float get() = actionsView.resources.displayMetrics.density
  override val isScrolling = false
  override val allowsSwipe: Boolean get() = actionsView.hasCurrentContent()

  override fun itemCellAt(x: Float, y: Float): ShadowListKitListCell? =
    actionsView.cell.takeIf { actionsView.hasCurrentContent() && x >= 0 && y >= 0 && x < actionsView.width && y < actionsView.height }

  override fun actions(cell: ShadowListKitListCell, leading: Boolean): ShadowListKitSwipeActionsConfiguration? =
    if (leading) actionsView.leadingSwipeActions else actionsView.trailingSwipeActions

  override fun isValid(cell: ShadowListKitListCell): Boolean = cell === actionsView.cell && actionsView.hasCurrentContent()
  override fun keyForCell(cell: ShadowListKitListCell): String? = null
  override fun beginSwipe() = actionsView.beginSwipe()
  override fun cancelHighlight() {}
  override fun addActionsView(view: View) = actionsView.addView(view, 0)
  override fun removeActionsView(view: View) = actionsView.removeView(view)
}
