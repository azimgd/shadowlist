package com.shadowlist.utils

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.view.View

/*
 * The three icons the rows draw: the grip of a reorderable row, the chevron of a row that opens
 * something and the check of a selected row. Each fills its view, which is square.
 */
internal class ShadowListUtilsIconView(context: Context, private val kind: Kind) : View(context) {
  enum class Kind { GRIP, CHEVRON, CHECK }

  private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
  private val path = Path()
  private var chevron: ShadowListUtilsChevronIcon? = null

  /*
   * The direction the chevron was last drawn for.
   */
  private var pathRightToLeft = false

  var color: Int
    get() = paint.color
    set(value) {
      if (paint.color == value) return
      paint.color = value
      if (kind == Kind.CHEVRON) buildPath()
      invalidate()
    }

  override fun onSizeChanged(width: Int, height: Int, previousWidth: Int, previousHeight: Int) {
    super.onSizeChanged(width, height, previousWidth, previousHeight)
    buildPath()
  }

  override fun onDraw(canvas: Canvas) {
    if (kind == Kind.GRIP) {
      // Three lines as wide as the icon, GRIP_GAP_SHARE of its size apart.
      paint.style = Paint.Style.FILL
      val thickness = GRIP_STROKE_DP.pxf
      val step = thickness + width * GRIP_GAP_SHARE
      val top = (height - (3 * thickness + 2 * width * GRIP_GAP_SHARE)) / 2f
      for (line in 0 until 3) {
        val y = top + line * step
        canvas.drawRoundRect(0f, y, width.toFloat(), y + thickness, thickness / 2f, thickness / 2f, paint)
      }
      return
    }
    if (kind == Kind.CHEVRON) {
      if (pathRightToLeft != ShadowListUtilsPlatform.isRightToLeft) buildPath()
      chevron?.draw(canvas)
      return
    }
    paint.style = Paint.Style.STROKE
    paint.strokeWidth = CHECK_STROKE_DP.pxf
    canvas.drawPath(path, paint)
  }

  /*
   * The icon's shape in a square of a size. The grip is filled and the others are stroked. A
   * chevron points left in a layout that runs from right to left.
   */
  private fun buildPath() {
    path.reset()
    val size = width.toFloat()
    pathRightToLeft = ShadowListUtilsPlatform.isRightToLeft
    when (kind) {
      Kind.GRIP -> Unit
      Kind.CHEVRON -> {
        chevron = ShadowListUtilsChevronIcon(color, size.dp, CHEVRON_STROKE_DP,
          if (pathRightToLeft) ShadowListUtilsChevronIcon.Direction.LEFT else ShadowListUtilsChevronIcon.Direction.RIGHT).apply { setBounds(0, 0, width, height) }
      }
      Kind.CHECK -> {
        // A short arm and a long one meeting at the bottom, the long one rising to the right.
        val short = size * CHECK_SHORT_SHARE * HALF_DIAGONAL
        val long = size * CHECK_LONG_SHARE * HALF_DIAGONAL
        val cornerX = size / 2f + (short - long) / 2f
        val cornerY = size / 2f + (short + long) / 2f - size * CHECK_LIFT_SHARE
        path.moveTo(cornerX - short, cornerY - short)
        path.lineTo(cornerX, cornerY)
        path.lineTo(cornerX + long, cornerY - long)
      }
    }
  }

  private companion object {
    const val HALF_DIAGONAL = 0.70710677f
    const val GRIP_STROKE_DP = 1.75f
    const val GRIP_GAP_SHARE = 0.22f
    const val CHEVRON_STROKE_DP = 2f
    const val CHECK_STROKE_DP = 2.2f
    const val CHECK_SHORT_SHARE = 0.26f
    const val CHECK_LONG_SHARE = 0.5f
    const val CHECK_LIFT_SHARE = 0.06f
  }
}
