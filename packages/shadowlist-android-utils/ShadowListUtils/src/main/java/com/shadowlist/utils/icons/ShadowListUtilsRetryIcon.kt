package com.shadowlist.utils

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable

class ShadowListUtilsRetryIcon(color: Int, private val size: Float = 20f, private val strokeWidth: Float = 1.8f) : Drawable() {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color }
  private val path = Path()

  override fun draw(canvas: Canvas) {
    val sizePx = size.pxf
    val stroke = strokeWidth.pxf
    canvas.save()
    canvas.translate(bounds.exactCenterX(), bounds.exactCenterY())
    val ring = sizePx * 0.6f
    val head = sizePx * 0.2f
    paint.style = Paint.Style.STROKE
    paint.strokeWidth = stroke
    canvas.drawArc(-ring / 2 + stroke / 2, -ring / 2 + stroke / 2, ring / 2 - stroke / 2,
      ring / 2 - stroke / 2, 0f, 270f, false, paint)
    canvas.translate(sizePx * 0.12f + head / 2, -sizePx * 0.3f + head / 2)
    canvas.rotate(100f)
    paint.style = Paint.Style.FILL
    canvas.drawPath(ShadowListUtilsChevronIcon.path(head, stroke, path), paint)
    canvas.restore()
  }

  override fun getIntrinsicWidth() = size.px
  override fun getIntrinsicHeight() = size.px
  override fun setAlpha(alpha: Int) { paint.alpha = alpha; invalidateSelf() }
  override fun setColorFilter(colorFilter: ColorFilter?) { paint.colorFilter = colorFilter; invalidateSelf() }
  @Deprecated("Deprecated in Java")
  override fun getOpacity() = PixelFormat.TRANSLUCENT
}
