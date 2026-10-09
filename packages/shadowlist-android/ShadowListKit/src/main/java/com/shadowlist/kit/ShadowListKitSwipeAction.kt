package com.shadowlist.kit

import android.graphics.Color
import android.graphics.drawable.Drawable

/*
 * A button shown behind a swiped row. The handler runs when the button is tapped or the row is
 * swiped all the way. Call completion with whether the action was performed. The row then
 * closes, unless the action removed it.
 */
class ShadowListKitSwipeAction(
  val style: Style,
  var title: String?,
  val handler: (action: ShadowListKitSwipeAction, completion: (performed: Boolean) -> Unit) -> Unit,
) {
  enum class Style { NORMAL, DESTRUCTIVE }

  var icon: Drawable? = null

  /*
   * Defaults to red for a destructive action and gray otherwise.
   */
  var backgroundColor: Int? = null

  internal val shownColor: Int
    get() = backgroundColor ?: if (style == Style.DESTRUCTIVE) Color.rgb(255, 59, 48) else Color.rgb(142, 142, 147)
}

/*
 * The actions of one side of a row. The first action is nearest the edge. With
 * performsFirstActionWithFullSwipe, the default, swiping the row all the way performs it, like
 * swipe to dismiss.
 */
class ShadowListKitSwipeActionsConfiguration(val actions: List<ShadowListKitSwipeAction>) {
  var performsFirstActionWithFullSwipe = true
}
