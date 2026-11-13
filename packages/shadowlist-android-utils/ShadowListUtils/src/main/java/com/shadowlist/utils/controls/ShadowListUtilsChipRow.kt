package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.util.SizeF
import android.view.View
import android.view.ViewGroup
import kotlin.math.ceil
import kotlin.math.roundToInt

/* Only native measurement and direction live here; shared wrapLayout places every child. */
class ShadowListUtilsChipRow(context: Context, children: List<View> = emptyList(), val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : ViewGroup(context) {
  var children = children.toList()
    set(value) { field = value.toList(); rebuild() }
  private var frames = emptyList<Rect>()

  init { rebuild() }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val available = MeasureSpec.getSize(widthMeasureSpec)
    val sizes = children.map { child ->
      child.measure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
      SizeF(child.measuredWidth.toFloat(), child.measuredHeight.toFloat())
    }
    val width = if (MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED) {
      sizes.sumOf { it.width.toDouble() }.toInt() + ((children.size - 1).coerceAtLeast(0) * theme.spacing.sm.px)
    } else available
    val layout = ShadowListUtilsWrapLayout(width.toFloat(), sizes, theme.spacing.sm.pxf)
    val direction = ShadowListUtilsDirection(width)
    frames = layout.frames.map { frame -> direction.place(Rect(frame.left.roundToInt(), frame.top.roundToInt(), frame.right.roundToInt(), frame.bottom.roundToInt())) }
    setMeasuredDimension(width, resolveSize(ceil(layout.height).toInt(), heightMeasureSpec))
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    for ((index, child) in children.withIndex()) frames.getOrNull(index)?.let { child.place(it) }
  }
  private fun rebuild() {
    for (index in childCount - 1 downTo 0) if (!children.contains(getChildAt(index))) removeViewAt(index)
    for ((index, child) in children.withIndex()) {
      if (child.parent !== this) { (child.parent as? ViewGroup)?.removeView(child); addView(child, index) }
      else if (indexOfChild(child) != index) { removeView(child); addView(child, index) }
    }
    requestLayout()
  }
}
