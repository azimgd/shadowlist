package com.shadowlist.kit

import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import androidx.core.view.ViewCompat

/* Native custom action ids compose with the caller's actions and resolve the current host. */
internal class ShadowListKitSwipeAccessibility(
  private val view: View,
  private val resolve: () -> Pair<ShadowListKitSwipeController, ShadowListKitListCell>?,
) {
  private var actions = emptyList<AccessibilityNodeInfo.AccessibilityAction>()
  private var entries = emptyList<Pair<Int, String>>()

  fun refresh() {
    val current = if (view.isAccessibilityVisible()) resolve() else null
    val next = current?.first?.accessibilityActions(current.second).orEmpty().mapIndexedNotNull { index, action ->
      action.title?.takeIf { it.isNotEmpty() }?.let { index to it }
    }
    if (entries == next) return
    clear()
    entries = next
    actions = next.mapNotNull { (index, label) ->
      val id = ViewCompat.addAccessibilityAction(view, label) { _, _ ->
        val target = if (view.isAccessibilityVisible()) resolve() else null
        target != null && target.first.performAccessibilityAction(target.second, index)
      }
      id.takeIf { it != View.NO_ID }?.let { AccessibilityNodeInfo.AccessibilityAction(it, label) }
    }
  }

  /* Include actions in the first node request that installs the native compatibility delegate. */
  fun appendTo(info: AccessibilityNodeInfo) {
    actions.forEach { info.addAction(it) }
  }

  fun clear() {
    actions.forEach { ViewCompat.removeAccessibilityAction(view, it.id) }
    actions = emptyList()
    entries = emptyList()
  }
}
