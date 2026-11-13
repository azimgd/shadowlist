package com.shadowlist.utils

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.View
import com.shadowlist.kit.ShadowListKitTextLayout

/*
 * Text drawn as one bar per line, for a row that stands in for content still loading. The words
 * are typeset in the real style and never shown.
 */
object ShadowListUtilsRedactedText {
  /*
   * One bar per line that holds text, in the layout's pixels: as wide as the line's text,
   * lineHeightPx * 0.62 tall and centered in its line. A blank line has no bar. A bar's corner
   * radius is half its height.
   */
  fun bars(layout: ShadowListKitTextLayout, lineHeightPx: Int): List<RectF> {
    val bars = ArrayList<RectF>(layout.lineCount)
    val barHeight = lineHeightPx * BAR_HEIGHT
    for (line in 0 until layout.lineCount) {
      val text = layout.rectForLine(line) ?: continue
      if (text.width() <= 0f) continue
      val top = text.top + (lineHeightPx - barHeight) / 2f
      bars.add(RectF(text.left, top, text.right, top + barHeight))
    }
    return bars
  }

  /*
   * Share of a line's height the bar covers, centered to keep the lines apart.
   */
  private const val BAR_HEIGHT = 0.62f
}

/*
 * Shows the bars of redacted text. Its frame is the frame the text would have.
 */
internal class ShadowListUtilsRedactedBarsView(context: Context) : View(context) {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG)

  var bars: List<RectF> = emptyList()
    set(value) {
      field = value
      invalidate()
    }

  var color: Int
    get() = paint.color
    set(value) {
      paint.color = value
      invalidate()
    }

  override fun onDraw(canvas: Canvas) {
    for (bar in bars) canvas.drawRoundRect(bar, bar.height() / 2f, bar.height() / 2f, paint)
  }
}
