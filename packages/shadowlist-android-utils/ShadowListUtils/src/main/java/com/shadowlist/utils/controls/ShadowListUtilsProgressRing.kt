package com.shadowlist.utils

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.View
import androidx.annotation.ColorInt
import androidx.annotation.Dimension

class ShadowListUtilsProgressRing(context: Context, fraction: Double, @Dimension(unit = Dimension.DP) size: Float = 52f, @Dimension(unit = Dimension.DP) strokeWidth: Float = 7f, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : View(context) {
  var fraction = fraction
    set(value) { field = value; invalidate() }
  @get:Dimension(unit = Dimension.DP)
  @setparam:Dimension(unit = Dimension.DP)
  var size = size
    set(value) { field = value; requestLayout(); invalidate() }
  @get:Dimension(unit = Dimension.DP)
  @setparam:Dimension(unit = Dimension.DP)
  var strokeWidth = strokeWidth
    set(value) { field = value; invalidate() }
  @get:ColorInt
  @setparam:ColorInt
  var color: Int? = null
    set(value) { field = value; invalidate() }
  @get:ColorInt
  @setparam:ColorInt
  var trackColor: Int? = null
    set(value) { field = value; invalidate() }
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }

  init { importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val length = size.coerceAtLeast(0f).px
    setMeasuredDimension(resolveSize(length, widthMeasureSpec), resolveSize(length, heightMeasureSpec))
  }
  override fun onDraw(canvas: Canvas) {
    val diameter = minOf(width, height).toFloat()
    val stroke = strokeWidth.coerceAtLeast(0f).pxf.coerceAtMost(diameter)
    if (diameter <= 0 || stroke <= 0) return
    val radius = (diameter - stroke) / 2f
    val circle = RectF(width / 2f - radius, height / 2f - radius, width / 2f + radius, height / 2f + radius)
    paint.strokeWidth = stroke; paint.strokeCap = Paint.Cap.BUTT; paint.color = trackColor ?: theme.colors.accentSoft
    canvas.drawArc(circle, -90f, 360f, false, paint)
    val value = ShadowListUtilsCore.clampFraction(fraction)
    if (value <= 0) return
    paint.strokeCap = Paint.Cap.ROUND; paint.color = color ?: theme.colors.accent
    canvas.drawArc(circle, -90f, (value * 360).toFloat(), false, paint)
  }
}
