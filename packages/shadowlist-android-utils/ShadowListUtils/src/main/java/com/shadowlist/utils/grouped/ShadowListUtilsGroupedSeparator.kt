package com.shadowlist.utils

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.view.View

class ShadowListUtilsGroupedSeparator(context: Context, inset: Float? = null, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : View(context) {
  var inset = inset
    set(value) { field = value; invalidate() }
  private val paint = Paint().apply { color = theme.colors.separator }
  init { importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) { setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), resolveSize(1, heightMeasureSpec)) }
  override fun onDraw(canvas: Canvas) {
    super.onDraw(canvas)
    val start = (inset ?: theme.grouped.rowInset).coerceAtLeast(0f).pxf.coerceAtMost(width.toFloat())
    if (layoutDirection == LAYOUT_DIRECTION_RTL) canvas.drawRect(0f, 0f, width - start, 1f, paint) else canvas.drawRect(start, 0f, width.toFloat(), 1f, paint)
  }
}
