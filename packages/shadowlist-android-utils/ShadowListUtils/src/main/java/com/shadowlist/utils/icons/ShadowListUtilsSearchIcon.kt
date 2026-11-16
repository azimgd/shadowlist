package com.shadowlist.utils

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable

internal class ShadowListUtilsSearchIcon(color: Int, private val size: Float = 17f, private val strokeWidth: Float = 1.8f) : Drawable() {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color }

  override fun draw(canvas: Canvas) {
    val sizePx = size.pxf
    val stroke = strokeWidth.pxf
    canvas.save()
    canvas.translate(bounds.exactCenterX() - sizePx / 2, bounds.exactCenterY() - sizePx / 2)
    val lens = sizePx * 0.62f
    val handle = sizePx * 0.34f
    val start = sizePx * 0.06f
    paint.style = Paint.Style.STROKE
    paint.strokeWidth = stroke
    canvas.drawOval(start + stroke / 2, start + stroke / 2, start + lens - stroke / 2, start + lens - stroke / 2, paint)
    paint.style = Paint.Style.FILL
    canvas.translate(start + lens * 0.85f - stroke / 2, start + lens * 0.85f + handle * 0.15f + stroke / 2)
    canvas.rotate(45f)
    canvas.drawRoundRect(0f, -stroke / 2, handle, stroke / 2, stroke, stroke, paint)
    canvas.restore()
  }

  override fun getIntrinsicWidth() = size.px
  override fun getIntrinsicHeight() = size.px
  override fun setAlpha(alpha: Int) { paint.alpha = alpha; invalidateSelf() }
  override fun setColorFilter(colorFilter: ColorFilter?) { paint.colorFilter = colorFilter; invalidateSelf() }
  @Deprecated("Deprecated in Java")
  override fun getOpacity() = PixelFormat.TRANSLUCENT
}
