package com.shadowlist.kit.example

import android.content.Context
import android.content.res.Resources
import android.view.View
import com.shadowlist.utils.ShadowListUtilsTheme
import kotlin.math.roundToInt

private val density = Resources.getSystem().displayMetrics.density

/*
 * Dp to pixels, for the views the example lays out itself.
 */
val Number.dp: Int get() = (toFloat() * density).roundToInt()
val Number.dpf: Float get() = toFloat() * density

/*
 * The library's light or dark theme by the device appearance. The template lists follow the
 * appearance themselves. This is for the views the example builds around them.
 */
val Context.listTheme: ShadowListUtilsTheme get() = ShadowListUtilsTheme.system(this).let {
  it.copy(grouped = it.grouped.copy(radius = 26f))
}

/*
 * Measure and place a child at a frame in one step. A child that only moved keeps its measure.
 */
fun View.place(left: Int, top: Int, width: Int, height: Int) {
  if (isLayoutRequested || measuredWidth != width || measuredHeight != height) {
    measure(
      View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY))
  }
  layout(left, top, left + width, top + height)
}
