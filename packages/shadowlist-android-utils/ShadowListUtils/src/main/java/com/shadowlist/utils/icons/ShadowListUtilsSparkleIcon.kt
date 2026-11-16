package com.shadowlist.utils

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable

internal class ShadowListUtilsSparkleIcon(color: Int, private val size: Float = 20f) : Drawable() {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color }

  override fun draw(canvas: Canvas) {
    val side = size.pxf * 0.46f
    val radius = side * 0.14f
    canvas.save()
    canvas.translate(bounds.exactCenterX(), bounds.exactCenterY())
    canvas.drawRoundRect(-side / 2, -side / 2, side / 2, side / 2, radius, radius, paint)
    canvas.rotate(45f)
    canvas.drawRoundRect(-side / 2, -side / 2, side / 2, side / 2, radius, radius, paint)
    canvas.restore()
  }

  override fun setAlpha(alpha: Int) { paint.alpha = alpha; invalidateSelf() }
  override fun setColorFilter(colorFilter: ColorFilter?) { paint.colorFilter = colorFilter; invalidateSelf() }
  @Deprecated("Deprecated in Java")
  override fun getOpacity() = PixelFormat.TRANSLUCENT
}
