package com.shadowlist.kit

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/*
 * The section index along a vertical list's trailing edge, drawn over the rows. Touching or
 * sliding over a title scrolls to its section.
 */
internal class SLKSectionIndex(private val list: SLKListView) {
  companion object {
    private const val TITLE_HEIGHT_DP = 16f
    private const val WIDTH_DP = 24f
    private const val TEXT_SP = 11f
  }

  var titles: List<String> = emptyList()
  var onSelect: ((Int) -> Unit)? = null

  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
    textAlign = Paint.Align.CENTER
    typeface = Typeface.DEFAULT_BOLD
    textSize = TEXT_SP * list.resources.displayMetrics.scaledDensity
    color = 0xFF007AFF.toInt()
  }
  private var owning = false
  private var selected = -1

  var color: Int
    get() = paint.color
    set(value) { paint.color = value }

  private val width: Float get() = WIDTH_DP * list.density
  private val titleHeight: Float get() = TITLE_HEIGHT_DP * list.density
  private val left: Float get() = (list.width - list.paddingRight).toFloat() - width
  private val top: Float get() {
    val area = (list.height - list.paddingTop - list.paddingBottom).toFloat()
    return list.paddingTop + max(0f, (area - titleHeight * titles.size) / 2)
  }

  private val isShown: Boolean get() = titles.isNotEmpty() && !list.horizontal

  /*
   * Own a touch that starts on the strip. Returns whether the index took the event.
   */
  fun handle(event: MotionEvent): Boolean {
    if (!isShown) return false
    when (event.actionMasked) {
      MotionEvent.ACTION_DOWN -> {
        owning = event.x >= left && event.x <= left + width
        selected = -1
        if (owning) {
          list.parent?.requestDisallowInterceptTouchEvent(true)
          select(event.y)
        }
      }
      MotionEvent.ACTION_MOVE -> if (owning) select(event.y)
      MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> if (owning) {
        owning = false
        return true
      }
    }
    return owning
  }

  private fun select(y: Float) {
    val index = min(max(floor((y - top) / titleHeight).toInt(), 0), titles.size - 1)
    if (index == selected) return
    selected = index
    list.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
    onSelect?.invoke(index)
  }

  fun draw(canvas: Canvas) {
    if (!isShown) return
    val x = list.scrollX + left + width / 2
    val baseline = (titleHeight - paint.ascent() - paint.descent()) / 2
    var y = list.scrollY + top
    for (title in titles) {
      canvas.drawText(title, x, y + baseline, paint)
      y += titleHeight
    }
  }
}
