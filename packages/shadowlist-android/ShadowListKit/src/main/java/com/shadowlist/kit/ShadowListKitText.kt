package com.shadowlist.kit

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.RectF
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import android.text.TextUtils
import android.view.View
import kotlin.math.ceil
import kotlin.math.min

// region Layout

/*
 * Text broken into lines once, for one width. Building one is safe on any thread, which moves
 * line breaking off the UI thread when a row's layout is computed ahead. A text view only draws it.
 */
class ShadowListKitTextLayout private constructor(val layout: StaticLayout) {
  val width: Int = lineWidth(layout)
  val height: Int = layout.height
  val lineCount: Int = layout.lineCount

  /*
   * The part of a line its text takes, without the space a line ends in, from the layout's top
   * left corner: as tall as the line, and as wide as nothing for a blank line. Null for an index
   * past the lines.
   */
  fun rectForLine(index: Int): RectF? {
    if (index < 0 || index >= lineCount) return null
    // The layout counts the spaces its last line ends in. A line cut short measures as it shows.
    val start = layout.getLineStart(index)
    var end = layout.getLineVisibleEnd(index)
    while (end > start && Character.isWhitespace(layout.text[end - 1])) --end
    val width = min(Layout.getDesiredWidth(layout.text, start, end, layout.paint), layout.getLineMax(index))
    val rightToLeft = layout.getParagraphDirection(index) == Layout.DIR_RIGHT_TO_LEFT
    val left = if (rightToLeft) layout.getLineRight(index) - width else layout.getLineLeft(index)
    return RectF(left, layout.getLineTop(index).toFloat(), left + width, layout.getLineBottom(index).toFloat())
  }

  companion object {
    /*
     * Layout's break strategies are the LineBreaker constants lint asks for, which need API 29.
     */
    @SuppressLint("WrongConstant")
    fun make(
      text: CharSequence,
      paint: TextPaint,
      width: Int,
      maxLines: Int = Int.MAX_VALUE,
      lineSpacingExtra: Float = 0f,
    ): ShadowListKitTextLayout {
      val builder = StaticLayout.Builder.obtain(text, 0, text.length, paint, width.coerceAtLeast(1))
        .setAlignment(Layout.Alignment.ALIGN_NORMAL)
        .setIncludePad(false)
        .setBreakStrategy(Layout.BREAK_STRATEGY_HIGH_QUALITY)
        .setHyphenationFrequency(Layout.HYPHENATION_FREQUENCY_NONE)
        .setLineSpacing(lineSpacingExtra, 1f)
        .setMaxLines(maxLines)
        .setEllipsize(if (maxLines == Int.MAX_VALUE) null else TextUtils.TruncateAt.END)
      return ShadowListKitTextLayout(builder.build())
    }

    private fun lineWidth(layout: StaticLayout): Int {
      var widest = 0f
      for (line in 0 until layout.lineCount) widest = maxOf(widest, layout.getLineWidth(line))
      return ceil(widest).toInt()
    }
  }
}

// endregion

// region View

/*
 * Draws a ShadowListKitTextLayout in textColor. Its size is the layout's size, which a row sets as its frame.
 */
class ShadowListKitTextView(context: Context) : View(context) {
  var textColor: Int = 0xFF000000.toInt()
    set(value) {
      if (field == value) return
      field = value
      invalidate()
    }

  var textLayout: ShadowListKitTextLayout? = null
    set(value) {
      if (field === value) return
      val resized = field?.width != value?.width || field?.height != value?.height
      field = value
      if (resized) requestLayout()
      invalidate()
    }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val layout = textLayout
    setMeasuredDimension(
      resolveSize(layout?.width ?: 0, widthMeasureSpec),
      resolveSize(layout?.height ?: 0, heightMeasureSpec))
  }

  /*
   * Layouts of one style share a paint. The color goes on right before drawing, which records it.
   */
  override fun onDraw(canvas: Canvas) {
    val layout = textLayout?.layout ?: return
    layout.paint.color = textColor
    layout.draw(canvas)
  }
}

// endregion
