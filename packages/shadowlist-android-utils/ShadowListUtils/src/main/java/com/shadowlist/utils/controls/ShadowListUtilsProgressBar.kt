package com.shadowlist.utils

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import androidx.annotation.ColorInt
import androidx.annotation.Dimension

class ShadowListUtilsProgressBar(context: Context, fraction: Double, accessibilityLabel: String, @Dimension(unit = Dimension.DP) height: Float = 6f, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : View(context) {
  var fraction = fraction
    set(value) { field = value; invalidate(); sendAccessibilityEvent(android.view.accessibility.AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) }
  var accessibilityLabel = accessibilityLabel
    set(value) { field = value; contentDescription = value }
  @get:ColorInt
  @setparam:ColorInt
  var color: Int? = null
    set(value) { field = value; invalidate() }
  private var barHeight = height
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG)

  init { contentDescription = accessibilityLabel; isFocusable = true; importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES }
  fun setHeight(@Dimension(unit = Dimension.DP) height: Float) { barHeight = height; requestLayout(); invalidate() }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(resolveSize(suggestedMinimumWidth, widthMeasureSpec), resolveSize(barHeight.coerceAtLeast(0f).px, heightMeasureSpec))
  }
  override fun onDraw(canvas: Canvas) {
    val radius = this.height / 2f
    val track = RectF(0f, 0f, width.toFloat(), this.height.toFloat())
    paint.color = theme.colors.fill; canvas.drawRoundRect(track, radius, radius, paint)
    canvas.save()
    val clip = Path().apply { addRoundRect(track, radius, radius, Path.Direction.CW) }
    canvas.clipPath(clip)
    if (layoutDirection == LAYOUT_DIRECTION_RTL) canvas.scale(-1f, 1f, width / 2f, this.height / 2f)
    val fill = RectF(0f, 0f, (width * ShadowListUtilsCore.clampFraction(fraction)).toFloat(), this.height.toFloat())
    paint.color = color ?: theme.colors.accent; canvas.drawRoundRect(fill, radius, radius, paint)
    canvas.restore()
  }
  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    info.className = android.widget.ProgressBar::class.java.name
    info.rangeInfo = AccessibilityNodeInfo.RangeInfo.obtain(AccessibilityNodeInfo.RangeInfo.RANGE_TYPE_INT, 0f, 100f, ShadowListUtilsCore.progressPercent(fraction).toFloat())
  }
}
