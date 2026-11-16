package com.shadowlist.kit

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.os.Build
import android.util.TypedValue
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.roundToInt

/*
 * The section index along a vertical list's trailing edge, drawn over the rows. Touching or
 * sliding over a title scrolls to its section. The strip's width, where the titles start and the
 * title under a touch come from the core's SectionIndex. A title is as tall as React Native's
 * index title on Android: one line of the font with its padding and TITLE_PADDING_DP above and
 * below.
 */
internal class ShadowListKitSectionIndex(private val list: ShadowListKitListView) {
  companion object {
    private const val TEXT_SP = 11f
    private const val TEXT_WEIGHT = 600
    private const val TITLE_PADDING_DP = 1f
  }

  var titles: List<String> = emptyList()
    set(value) {
      field = value
      titlesTopArea = -1.0
    }
  var onSelect: ((Int) -> Unit)? = null

  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
    typeface = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) Typeface.create(Typeface.DEFAULT, TEXT_WEIGHT, false) else Typeface.DEFAULT_BOLD
    textSize = ceil(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, TEXT_SP, list.resources.displayMetrics))
    isSubpixelText = true
    color = 0xFF007AFF.toInt()
  }
  private var owning = false
  private var selected = -1

  var color: Int
    get() = paint.color
    set(value) { paint.color = value }

  private val width: Float get() = (ShadowListKitCore.SECTION_INDEX_WIDTH_DP * list.density).toFloat()
  private val titlePadding: Float get() = TITLE_PADDING_DP * list.density
  private val titleHeight: Float get() = paint.fontMetricsInt.let { it.bottom - it.top } + 2 * titlePadding

  /*
   * The core lays titles SECTION_INDEX_TITLE_HEIGHT_DP apart at a scale. This scale makes that
   * the height of a title here.
   */
  private val scale: Double get() = titleHeight / ShadowListKitCore.SECTION_INDEX_TITLE_HEIGHT_DP
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
      titlesTop = ShadowListKitCore.sectionIndexTitlesTop(area, titles.size, scale).toFloat()
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
    val index = ShadowListKitCore.sectionIndexTitleAt((y - list.paddingTop).toDouble(), area, titles.size, scale)
    if (index == selected) return
    selected = index
    list.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
    onSelect?.invoke(index)
  }

  /*
   * Titles land on whole pixels the way React Native places them: the column's top is rounded,
   * and each title's origin is floored inside it, down the column and across the strip.
   */
  fun draw(canvas: Canvas) {
    if (!isShown) return
    val titleHeight = titleHeight
    val column = list.scrollY + top.roundToInt()
    val baseline = floor(titlePadding) - paint.fontMetricsInt.top
    for ((index, title) in titles.withIndex()) {
      val x = list.scrollX + left + floor((width - ceil(paint.measureText(title))) / 2)
      canvas.drawText(title, x, column + floor(index * titleHeight) + baseline, paint)
    }
  }
}
