package com.shadowlist.utils

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable

internal class ShadowListUtilsChevronIcon(
  color: Int,
  private val size: Float = 17f,
  private val strokeWidth: Float = 2f,
  private val direction: Direction = Direction.RIGHT,
) : Drawable() {
  enum class Direction { RIGHT, DOWN, LEFT, UP }

  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color }
  private val path = Path()

  override fun onBoundsChange(bounds: android.graphics.Rect) {
    val side = size.pxf * 0.42f
    val k = side * 0.3535f
    val stroke = strokeWidth.pxf
    path(side, stroke, path)
    val matrix = android.graphics.Matrix()
    when (direction) {
      Direction.RIGHT -> { matrix.postRotate(45f); matrix.postTranslate(-k, 0f) }
      Direction.DOWN -> { matrix.postRotate(135f); matrix.postTranslate(0f, -k) }
      Direction.LEFT -> { matrix.postRotate(225f); matrix.postTranslate(k, 0f) }
      Direction.UP -> { matrix.postRotate(-45f); matrix.postTranslate(0f, k) }
    }
    matrix.postTranslate(bounds.exactCenterX(), bounds.exactCenterY())
    path.transform(matrix)
  }

  override fun draw(canvas: Canvas) { canvas.drawPath(path, paint) }

  override fun getIntrinsicWidth() = size.px
  override fun getIntrinsicHeight() = size.px
  override fun setAlpha(alpha: Int) { paint.alpha = alpha; invalidateSelf() }
  override fun setColorFilter(colorFilter: ColorFilter?) { paint.colorFilter = colorFilter; invalidateSelf() }
  @Deprecated("Deprecated in Java")
  override fun getOpacity() = PixelFormat.TRANSLUCENT

  companion object {
    /*
     * The top and right borders of a square of a side, centered on the origin. The arrow and
     * retry icons draw their heads with it.
     */
    fun path(side: Float, strokeWidth: Float, path: Path): Path {
      path.reset()
      path.moveTo(-side / 2, -side / 2)
      path.lineTo(side / 2, -side / 2)
      path.lineTo(side / 2, side / 2)
      path.lineTo(side / 2 - strokeWidth, side / 2)
      path.lineTo(side / 2 - strokeWidth, -side / 2 + strokeWidth)
      path.lineTo(-side / 2, -side / 2 + strokeWidth)
      path.close()
      return path
    }
  }
}
