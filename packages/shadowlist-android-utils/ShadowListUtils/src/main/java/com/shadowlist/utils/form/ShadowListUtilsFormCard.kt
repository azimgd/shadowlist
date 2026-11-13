package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.widget.LinearLayout

class ShadowListUtilsFormCard(context: Context, children: List<View> = emptyList(), val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : LinearLayout(context) {
  var children = children.toList()
    set(value) {
      if (field.size == value.size && field.indices.all { field[it] === value[it] }) return
      field = value.toList(); rebuild()
    }
  private val separators = mutableListOf<View>()
  init {
    orientation = VERTICAL; clipToOutline = true
    background = GradientDrawable().apply { setColor(theme.colors.groupedCell); cornerRadius = theme.grouped.radius.pxf }
    rebuild()
  }
  private fun rebuild() {
    while (separators.size < (children.size - 1).coerceAtLeast(0)) {
      separators.add(android.widget.FrameLayout(context).apply {
        layoutParams = LayoutParams(LayoutParams.MATCH_PARENT, 1)
        addView(View(context).apply { setBackgroundColor(theme.colors.separator) }, android.widget.FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, 1).apply { marginStart = theme.grouped.rowInset.px })
      })
    }
    val arranged = mutableListOf<View>()
    children.forEachIndexed { index, child -> if (index > 0) arranged.add(separators[index - 1]); arranged.add(child) }
    replaceChildren(arranged)
  }
}
