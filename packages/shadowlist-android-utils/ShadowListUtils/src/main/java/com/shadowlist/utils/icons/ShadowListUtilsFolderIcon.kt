package com.shadowlist.utils

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable

class ShadowListUtilsFolderIcon(color: Int, private val size: Float = 18f) : Drawable() {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color }

  override fun draw(canvas: Canvas) {
    val sizePx = size.pxf
    canvas.save()
    canvas.translate(bounds.exactCenterX(), bounds.exactCenterY())
    val top = -sizePx * 0.82f / 2
    canvas.drawRoundRect(-sizePx / 2, top, -sizePx / 2 + sizePx * 0.44f, top + sizePx * 0.28f,
      2.5f.pxf, 2.5f.pxf, paint)
    canvas.drawRoundRect(-sizePx / 2, top + sizePx * 0.16f, sizePx / 2, top + sizePx * 0.82f,
      3f.pxf, 3f.pxf, paint)
    canvas.restore()
  }

  override fun getIntrinsicWidth() = size.px
  override fun getIntrinsicHeight() = (size * 0.82f).px
  override fun setAlpha(alpha: Int) { paint.alpha = alpha; invalidateSelf() }
  override fun setColorFilter(colorFilter: ColorFilter?) { paint.colorFilter = colorFilter; invalidateSelf() }
  @Deprecated("Deprecated in Java")
  override fun getOpacity() = PixelFormat.TRANSLUCENT
}
