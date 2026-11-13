package com.shadowlist.utils

import android.content.Context
import android.view.View
import android.view.ViewGroup

/* Standalone total-vote footer, also composed by the list's owned default footer. */
class ShadowListUtilsPollFooter(context: Context, totalVotes: Double, theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : ViewGroup(context) {
  var totalVotes = totalVotes
    set(value) { field = value; rebuild() }
  var labels = ShadowListUtilsPollLabels()
    set(value) { field = value; rebuild() }
  var theme = theme
    set(value) { field = value; rebuild() }
  private var footer: ShadowListUtilsListFooter? = null
  private val separator = View(context)
  init { addView(separator); rebuild() }
  private fun rebuild() {
    footer?.let(::removeView)
    footer = ShadowListUtilsListFooter(context, labels.totalVotes(totalVotes), theme.copy(spacing = theme.spacing.copy(xl = theme.spacing.lg))).also(::addView)
    separator.setBackgroundColor(theme.colors.separator); requestLayout()
  }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    footer?.measure(widthMeasureSpec, heightMeasureSpec)
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), footer?.measuredHeight ?: 0)
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    footer?.layout(0, 0, width, height); separator.layout(0, 0, width, 1); separator.bringToFront()
  }
}
