package com.shadowlist.kit

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.view.HapticFeedbackConstants
import android.view.MotionEvent

/*
 * The section index along a vertical list's trailing edge, drawn over the rows. Touching or
 * sliding over a title scrolls to its section. Sizes and the title under a touch come from the
 * core's SectionIndex, shared with iOS.
 */
internal class ShadowListKitSectionIndex(private val list: ShadowListKitListView) {
  companion object {
    private const val TEXT_SP = 11f
  }

  var titles: List<String> = emptyList()
    set(value) {
      field = value
      titlesTopArea = -1.0
    }
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

  private val width: Float get() = (ShadowListKitCore.SECTION_INDEX_WIDTH_DP * list.density).toFloat()
  private val titleHeight: Float get() = (ShadowListKitCore.SECTION_INDEX_TITLE_HEIGHT_DP * list.density).toFloat()
  private val left: Float get() = (list.width - list.paddingRight).toFloat() - width
  private val area: Double get() = (list.height - list.paddingTop - list.paddingBottom).toDouble()

  /*
   * Where the titles start in the area, asked of the core once per area and title count.
   */
  private var titlesTopArea = -1.0
  private var titlesTop = 0f
  private val top: Float get() {
    if (titlesTopArea != area) {
      titlesTopArea = area
      titlesTop = ShadowListKitCore.sectionIndexTitlesTop(area, titles.size, list.density.toDouble()).toFloat()
    }
    return list.paddingTop + titlesTop
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
    val index = ShadowListKitCore.sectionIndexTitleAt((y - list.paddingTop).toDouble(), area, titles.size, list.density.toDouble())
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
