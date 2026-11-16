package com.shadowlist.utils

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable

internal class ShadowListUtilsArrowUpIcon(color: Int, private val size: Float = 20f, private val strokeWidth: Float = 2.2f) : Drawable() {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color }
  private val path = Path()

  override fun draw(canvas: Canvas) {
    val sizePx = size.pxf
    val stroke = strokeWidth.pxf
    canvas.save()
    canvas.translate(bounds.exactCenterX(), bounds.exactCenterY())
    canvas.drawRoundRect(-stroke / 2, -sizePx * 0.24f, stroke / 2, sizePx * 0.32f, stroke, stroke, paint)
    val head = sizePx * 0.36f
    canvas.translate(0f, -sizePx * 0.34f + head / 2)
    canvas.rotate(-45f)
    canvas.drawPath(ShadowListUtilsChevronIcon.path(head, stroke, path), paint)
    canvas.restore()
  }

  override fun setAlpha(alpha: Int) { paint.alpha = alpha; invalidateSelf() }
  override fun setColorFilter(colorFilter: ColorFilter?) { paint.colorFilter = colorFilter; invalidateSelf() }
  @Deprecated("Deprecated in Java")
  override fun getOpacity() = PixelFormat.TRANSLUCENT
}
