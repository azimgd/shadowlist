package com.shadowlist.utils

import android.content.Context
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView

class ShadowListUtilsFormRow(context: Context, label: String, children: List<View>, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : LinearLayout(context) {
  var label = label
    set(value) { field = value; labelView.text = value; requestLayout() }
  var children = children.toList()
    set(value) { field = value.toList(); applyLayout() }
  var stacked = false
    set(value) { field = value; applyLayout() }
  private val labelView = TextView(context).apply { text = label; setTextColor(theme.colors.label); textSize = theme.typography.body.fontSize; maxLines = 2 }
  private val control = LinearLayout(context).apply { orientation = HORIZONTAL; gravity = Gravity.END or Gravity.CENTER_VERTICAL }
  init { minimumHeight = theme.grouped.rowHeight.px; addView(labelView); addView(control); applyLayout() }
  override fun onConfigurationChanged(newConfig: android.content.res.Configuration) { super.onConfigurationChanged(newConfig); applyLayout() }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    labelView.maxWidth = if (stacked || resources.configuration.fontScale >= 1.3f) Int.MAX_VALUE else ((MeasureSpec.getSize(widthMeasureSpec) - 2 * theme.grouped.rowInset.px) / 2).coerceAtLeast(0)
    super.onMeasure(widthMeasureSpec, heightMeasureSpec)
  }
  private fun applyLayout() {
    val over = stacked || resources.configuration.fontScale >= 1.3f
    orientation = if (over) VERTICAL else HORIZONTAL
    gravity = if (over) Gravity.FILL_HORIZONTAL else Gravity.CENTER_VERTICAL
    setPadding(theme.grouped.rowInset.px, (if (over) 10f else 6f).px, theme.grouped.rowInset.px, (if (over) 10f else 6f).px)
    labelView.layoutParams = LayoutParams(if (over) LayoutParams.MATCH_PARENT else LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT).apply {
      if (over) bottomMargin = theme.spacing.sm.px else marginEnd = theme.spacing.md.px
    }
    control.layoutParams = if (over) LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT) else LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f)
    control.gravity = (if (over) Gravity.START else Gravity.END) or Gravity.CENTER_VERTICAL
    for (child in children) if (child is ShadowListUtilsFormInput) {
      child.stacked = over
      child.layoutParams = LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f)
    }
    control.replaceChildren(children)
  }
}
