package com.shadowlist.utils

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable

class ShadowListUtilsCopyIcon(color: Int, private val size: Float = 20f, private val strokeWidth: Float = 1.6f) : Drawable() {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color }

  override fun draw(canvas: Canvas) {
    val sizePx = size.pxf
    val stroke = strokeWidth.pxf
    canvas.save()
    canvas.translate(bounds.exactCenterX(), bounds.exactCenterY())
    val sheet = sizePx * 0.5f
    paint.style = Paint.Style.STROKE
    paint.strokeWidth = stroke
    for (offset in listOf(-0.34f, -0.16f)) {
      val start = sizePx * offset
      canvas.drawRoundRect(start + stroke / 2, start + stroke / 2, start + sheet - stroke / 2,
        start + sheet - stroke / 2, sheet * 0.22f, sheet * 0.22f, paint)
    }
    canvas.restore()
  }

  override fun getIntrinsicWidth() = size.px
  override fun getIntrinsicHeight() = size.px
  override fun setAlpha(alpha: Int) { paint.alpha = alpha; invalidateSelf() }
  override fun setColorFilter(colorFilter: ColorFilter?) { paint.colorFilter = colorFilter; invalidateSelf() }
  @Deprecated("Deprecated in Java")
  override fun getOpacity() = PixelFormat.TRANSLUCENT
}
