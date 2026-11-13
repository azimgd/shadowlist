package com.shadowlist.utils

import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout

/* Stable native children keep their selection/focus when only controlled values change. */
internal fun LinearLayout.replaceChildren(children: List<View>) {
  for (index in childCount - 1 downTo 0) {
    val child = getChildAt(index)
    if (children.none { it === child }) removeViewAt(index)
  }
  children.forEachIndexed { index, child ->
    if (index < childCount && getChildAt(index) === child) return@forEachIndexed
    (child.parent as? ViewGroup)?.removeView(child)
    addView(child, index)
  }
}
