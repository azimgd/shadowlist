package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.widget.LinearLayout

class ShadowListUtilsGroupedCard(context: Context, children: List<View> = emptyList(), val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : LinearLayout(context) {
  var children = children.toList()
    set(value) { field = value.toList(); stack.replaceChildren(field) }
  private val stack = LinearLayout(context).apply {
    orientation = VERTICAL; clipToOutline = true
    background = GradientDrawable().apply { setColor(theme.colors.groupedCell); cornerRadius = theme.grouped.radius.pxf }
  }
  init {
    orientation = VERTICAL; setPadding(theme.grouped.inset.px, 0, theme.grouped.inset.px, 0)
    addView(stack, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    stack.replaceChildren(this.children)
  }
}
