package com.shadowlist.utils

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable

class ShadowListUtilsDocIcon(color: Int, private val size: Float = 18f, private val strokeWidth: Float = 1.6f) : Drawable() {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color }

  override fun draw(canvas: Canvas) {
    val sizePx = size.pxf
    val stroke = strokeWidth.pxf
    canvas.save()
    canvas.translate(bounds.exactCenterX(), bounds.exactCenterY())
    val w = sizePx * 0.72f
    val h = sizePx * 0.9f
    paint.style = Paint.Style.STROKE
    paint.strokeWidth = stroke
    canvas.drawRoundRect(-w / 2 + stroke / 2, -h / 2 + stroke / 2,
      w / 2 - stroke / 2, h / 2 - stroke / 2, 2.5f.pxf, 2.5f.pxf, paint)
    paint.style = Paint.Style.FILL
    val x = -w / 2 + stroke + w * 0.18f
    val lineWidth = w - 2 * (stroke + w * 0.18f)
    val top = -(2 * stroke + h * 0.16f) / 2
    canvas.drawRoundRect(x, top, x + lineWidth, top + stroke, stroke, stroke, paint)
    canvas.drawRoundRect(x, top + stroke + h * 0.16f, x + lineWidth * 0.7f,
      top + 2 * stroke + h * 0.16f, stroke, stroke, paint)
    canvas.restore()
  }

  override fun getIntrinsicWidth() = size.px
  override fun getIntrinsicHeight() = size.px
  override fun setAlpha(alpha: Int) { paint.alpha = alpha; invalidateSelf() }
  override fun setColorFilter(colorFilter: ColorFilter?) { paint.colorFilter = colorFilter; invalidateSelf() }
  @Deprecated("Deprecated in Java")
  override fun getOpacity() = PixelFormat.TRANSLUCENT
}
