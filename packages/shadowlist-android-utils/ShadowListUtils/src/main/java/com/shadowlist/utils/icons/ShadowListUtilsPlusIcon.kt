package com.shadowlist.utils

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable
import kotlin.math.roundToInt

class ShadowListUtilsPlusIcon(color: Int, private val size: Float = 20f, private val strokeWidth: Float = 2f) : Drawable() {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color }

  override fun draw(canvas: Canvas) {
    val sizePx = size.pxf
    val stroke = strokeWidth.pxf
    val bar = sizePx * 0.56f
    // React Native lays the two bars out as views, which land on whole pixels.
    val left = (bounds.exactCenterX() - sizePx / 2).roundToInt()
    val top = (bounds.exactCenterY() - sizePx / 2).roundToInt()
    val along = ((sizePx - bar) / 2).roundToInt().toFloat()
    val across = ((sizePx - stroke) / 2).roundToInt().toFloat()
    val length = bar.roundToInt()
    val thickness = stroke.roundToInt()
    canvas.drawRoundRect(left + along, top + across, left + along + length, top + across + thickness, stroke, stroke, paint)
    canvas.drawRoundRect(left + across, top + along, left + across + thickness, top + along + length, stroke, stroke, paint)
  }

  override fun getIntrinsicWidth() = size.px
  override fun getIntrinsicHeight() = size.px
  override fun setAlpha(alpha: Int) { paint.alpha = alpha; invalidateSelf() }
  override fun setColorFilter(colorFilter: ColorFilter?) { paint.colorFilter = colorFilter; invalidateSelf() }
  @Deprecated("Deprecated in Java")
  override fun getOpacity() = PixelFormat.TRANSLUCENT
}
