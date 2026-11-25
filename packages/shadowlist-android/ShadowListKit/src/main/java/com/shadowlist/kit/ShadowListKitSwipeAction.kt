package com.shadowlist.kit

import android.graphics.drawable.Drawable

/*
 * A button shown behind a swiped item. The handler runs when the button is tapped or the item
 * is swiped all the way. Call completion with whether the action was performed. The item then
 * closes, unless the action deleted it.
 */
class ShadowListKitSwipeAction(
  val style: Style,
  var title: String?,
  val handler: (action: ShadowListKitSwipeAction, completion: (performed: Boolean) -> Unit) -> Unit
) {
  enum class Style { NORMAL, DESTRUCTIVE }

  var image: Drawable? = null

  /*
   * Defaults to red for a destructive action and gray otherwise.
   */
  var backgroundColor: Int? = null

  internal val shownColor: Int
    get() = backgroundColor
      ?: if (style == Style.DESTRUCTIVE) {
        ShadowListKitCore.SWIPE_ACTION_COLOR_DESTRUCTIVE
      } else {
        ShadowListKitCore.SWIPE_ACTION_COLOR_NORMAL
      }
}

/*
 * The actions of one side of an item. The first action is nearest the edge. With
 * performsFirstActionWithFullSwipe, the default, swiping the item all the way performs it, like
 * swipe to dismiss.
 */
class ShadowListKitSwipeActionsConfiguration(val actions: List<ShadowListKitSwipeAction>) {
  var performsFirstActionWithFullSwipe = true
}
