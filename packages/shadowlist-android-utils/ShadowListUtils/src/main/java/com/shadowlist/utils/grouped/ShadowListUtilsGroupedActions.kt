package com.shadowlist.utils

import android.content.Context
import android.view.Menu
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import com.shadowlist.kit.ShadowListKitItemActionsView
import com.shadowlist.kit.ShadowListKitSwipeAction
import com.shadowlist.kit.ShadowListKitSwipeActionsConfiguration

/* The existing native action surface around ordinary caller-owned grouped content. */
class ShadowListUtilsGroupedActions(
  context: Context,
  children: List<View>,
  theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
) : FrameLayout(context) {
  var children = children.toList()
    set(value) { field = value.toList(); content.replaceChildren(field); requestLayout() }
  var theme = theme
    set(value) { field = value; updateActions() }
  var leadingSwipeActions: ShadowListKitSwipeActionsConfiguration? = null
    set(value) { field = value; updateActions() }
  var trailingSwipeActions: ShadowListKitSwipeActionsConfiguration? = null
    set(value) { field = value; updateActions() }
  var contextMenu: ((Menu) -> Boolean)? = null
    set(value) { field = value; actions.contextMenu = value }
  var onContextMenuWillShow: (() -> Unit)? = null
    set(value) { field = value; actions.onContextMenuWillShow = value }

  private val actions = ShadowListKitItemActionsView(context)
  private val content = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }

  init {
    content.replaceChildren(this.children)
    actions.contentView.addView(content, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    addView(actions, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
  }

  fun closeSwipeActions(animated: Boolean = true) = actions.closeSwipeActions(animated)

  internal fun useExternalAccessibilityOwner() {
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
    actions.importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
    content.importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
  }


  private fun updateActions() {
    actions.leadingSwipeActions = themed(leadingSwipeActions)
    actions.trailingSwipeActions = themed(trailingSwipeActions)
  }

  /* Copy only the implicit destructive color, retaining explicit colors and the caller model. */
  private fun themed(configuration: ShadowListKitSwipeActionsConfiguration?): ShadowListKitSwipeActionsConfiguration? {
    if (configuration == null) return null
    val mapped = configuration.actions.map { action ->
      if (action.style != ShadowListKitSwipeAction.Style.DESTRUCTIVE || action.backgroundColor != null) action
      else ShadowListKitSwipeAction(action.style, action.title, action.handler).also { copy ->
        copy.image = action.image
        copy.backgroundColor = theme.colors.red
      }
    }
    return ShadowListKitSwipeActionsConfiguration(mapped).also {
      it.performsFirstActionWithFullSwipe = configuration.performsFirstActionWithFullSwipe
    }
  }
}
