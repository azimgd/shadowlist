package com.shadowlist.kit

import android.content.Context
import android.graphics.Color
import android.graphics.Rect
import android.graphics.drawable.ColorDrawable
import android.view.Gravity
import android.view.ViewGroup
import android.widget.TextView
import kotlin.math.roundToInt

/*
 * The buttons behind a swiped row, for both sides. It sits under the row's cell with the
 * cell's frame. The side being revealed shows its buttons stretched over the gap the row
 * leaves. Past the full swipe point the first button fills all of it.
 */
internal class ShadowListKitSwipeActionsView(
  context: Context,
  val leading: ShadowListKitSwipeActionsConfiguration?,
  val trailing: ShadowListKitSwipeActionsConfiguration?,
  private val horizontal: Boolean,
  density: Float,
  private val onAction: (ShadowListKitSwipeAction) -> Unit
) : ViewGroup(context) {
  private val leadingButtons = buttons(leading)
  private val trailingButtons = buttons(trailing)
  val leadingSizes = sizes(leadingButtons, density)
  val trailingSizes = sizes(trailingButtons, density)
  private val clip = Rect()

  private fun buttons(configuration: ShadowListKitSwipeActionsConfiguration?): List<TextView> =
    configuration?.actions?.map { action ->
      TextView(context).apply {
        text = action.title
        setTextColor(Color.WHITE)
        textSize = 15f
        gravity = Gravity.CENTER
        maxLines = 1
        setBackgroundColor(action.shownColor)
        action.image?.let { setCompoundDrawablesWithIntrinsicBounds(null, it, null, null) }
        contentDescription = action.title
        setOnClickListener { onAction(action) }
        addView(this)
      }
    } ?: emptyList()

  private fun sizes(buttons: List<TextView>, density: Float): DoubleArray = DoubleArray(buttons.size) {
    val button = buttons[it]
    button.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED)
    val fitted = if (horizontal) button.measuredHeight else button.measuredWidth
    ShadowListKitCore.swipeButtonSize(fitted.toDouble(), density.toDouble())
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.getSize(heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {}

  /*
   * Place the buttons for a row moved offset across the axis, at the spans the core wrote into
   * values, see ShadowListKitSwipeReveal.OUT_*.
   */
  fun layoutFor(offset: Float, values: DoubleArray) {
    val leadingSide = offset > 0
    val shown = if (leadingSide) leadingButtons else trailingButtons
    val other = if (leadingSide) trailingButtons else leadingButtons
    for (button in other) button.visibility = INVISIBLE
    val alongSize = if (horizontal) width else height
    for ((at, button) in shown.withIndex()) {
      // Each edge is rounded on its own, which keeps the buttons touching.
      val span = ShadowListKitSwipeReveal.OUT_SPANS + at * 2
      val start = values[span].roundToInt()
      val end = (values[span] + values[span + 1]).roundToInt()
      val size = end - start
      button.visibility = if (size > 0) VISIBLE else INVISIBLE
      if (size > 0) {
        val w = if (horizontal) alongSize else size
        val h = if (horizontal) size else alongSize
        button.measure(
          MeasureSpec.makeMeasureSpec(w, MeasureSpec.EXACTLY),
          MeasureSpec.makeMeasureSpec(h, MeasureSpec.EXACTLY)
        )
        if (horizontal) button.layout(0, start, alongSize, end) else button.layout(start, 0, end, alongSize)
      }
    }
    shown.firstOrNull()?.let { setBackgroundColor((it.background as? ColorDrawable)?.color ?: 0) }
    // Only the gap shows the buttons' color.
    val gapStart = values[ShadowListKitSwipeReveal.OUT_REVEALED_START].roundToInt()
    val gapEnd = (
      values[ShadowListKitSwipeReveal.OUT_REVEALED_START] +
        values[ShadowListKitSwipeReveal.OUT_REVEALED_SIZE]
      ).roundToInt()
    if (horizontal) clip.set(0, gapStart, alongSize, gapEnd) else clip.set(gapStart, 0, gapEnd, alongSize)
    clipBounds = clip
  }

  /*
   * Whether a point in the list's scrolled coordinates hits a shown button.
   */
  fun hasButtonAt(x: Float, y: Float): Boolean {
    val localX = (x - left).toInt()
    val localY = (y - top).toInt()
    for (button in leadingButtons + trailingButtons) {
      if (button.visibility == VISIBLE && localX >= button.left && localX < button.right &&
        localY >= button.top && localY < button.bottom
      ) {
        return true
      }
    }
    return false
  }
}
