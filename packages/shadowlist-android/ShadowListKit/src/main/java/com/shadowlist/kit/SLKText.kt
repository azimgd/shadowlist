package com.shadowlist.kit

import android.content.Context
import android.graphics.Canvas
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import android.text.TextUtils
import android.view.View
import kotlin.math.ceil

// region Layout

/*
 * Text broken into lines once, for one width. Building one is safe on any thread, which moves
 * line breaking off the UI thread when a row's layout is computed ahead. A text view only draws it.
 */
class SLKTextLayout private constructor(val layout: StaticLayout) {
  val width: Int = lineWidth(layout)
  val height: Int = layout.height

  companion object {
    fun make(
      text: CharSequence,
      paint: TextPaint,
      width: Int,
      maxLines: Int = Int.MAX_VALUE,
      lineSpacingExtra: Float = 0f,
    ): SLKTextLayout {
      val builder = StaticLayout.Builder.obtain(text, 0, text.length, paint, width.coerceAtLeast(1))
        .setAlignment(Layout.Alignment.ALIGN_NORMAL)
        .setIncludePad(false)
        .setLineSpacing(lineSpacingExtra, 1f)
        .setMaxLines(maxLines)
        .setEllipsize(if (maxLines == Int.MAX_VALUE) null else TextUtils.TruncateAt.END)
      return SLKTextLayout(builder.build())
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
 * Draws an SLKTextLayout in textColor. Its size is the layout's size, which a row sets as its frame.
 */
class SLKTextView(context: Context) : View(context) {
  var textColor: Int = 0xFF000000.toInt()
    set(value) {
      if (field == value) return
      field = value
      invalidate()
    }

  var textLayout: SLKTextLayout? = null
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
