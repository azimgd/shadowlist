package com.shadowlist.kit

import android.view.View
import android.view.ViewGroup

/*
 * The native surface around a swipe. The controller owns reveal/session state; the host
 * retains hit testing, current action configuration and its own scroll/reorder policy.
 */
internal interface ShadowListKitSwipeHost {
  val view: ViewGroup
  val horizontal: Boolean
  val density: Float
  val isScrolling: Boolean
  val allowsSwipe: Boolean

  fun itemCellAt(x: Float, y: Float): ShadowListKitListCell?
  fun actions(cell: ShadowListKitListCell, leading: Boolean): ShadowListKitSwipeActionsConfiguration?
  fun isValid(cell: ShadowListKitListCell): Boolean
  fun keyForCell(cell: ShadowListKitListCell): String?
  fun beginSwipe()
  fun cancelHighlight()
  fun addActionsView(view: View)
  fun removeActionsView(view: View)
}
