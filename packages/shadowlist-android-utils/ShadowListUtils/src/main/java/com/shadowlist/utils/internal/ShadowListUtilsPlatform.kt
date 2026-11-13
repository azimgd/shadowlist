package com.shadowlist.utils

import android.content.Context
import android.content.pm.ApplicationInfo
import android.content.res.Resources
import android.graphics.Canvas
import android.graphics.Outline
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.os.Build
import android.util.TypedValue
import android.view.View
import android.view.ViewOutlineProvider
import android.view.accessibility.AccessibilityNodeInfo
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.roundToInt

/*
 * What a layout needs from the device: the reader's text size, the layout direction and the
 * window's height. A list resolves the last two on the UI thread, and a row's layout reads the
 * resolved values on any thread.
 */
internal object ShadowListUtilsPlatform {
  // region Text size

  /*
   * How much larger than its theme size text is set, by the reader's text size setting. 1 at
   * the system's default size.
   */
  val textScale: Float
    get() = textScaleOverride ?: Resources.getSystem().configuration.fontScale

  /*
   * Whether the reader's text size is large enough for rows to give their titles more lines.
   */
  val isLargeText: Boolean get() = textScale >= LARGE_TEXT_SCALE

  /*
   * Stands in for the reader's text size in a test. Null follows the device again.
   */
  @Volatile
  var textScaleOverride: Float? = null

  // endregion

  // region Direction

  /*
   * Whether the app lays out from right to left. A row then mirrors its frames. It is the
   * direction a list's view last resolved to, and the device's before any list did.
   */
  val isRightToLeft: Boolean
    get() = isRightToLeftOverride ?: resolvedRightToLeft
      ?: (Resources.getSystem().configuration.layoutDirection == View.LAYOUT_DIRECTION_RTL)

  /*
   * Stands in for the layout direction in a test. Null follows the device again.
   */
  @Volatile
  var isRightToLeftOverride: Boolean? = null

  @Volatile
  private var resolvedRightToLeft: Boolean? = null

  /*
   * A list tells the direction its view resolved to, on the UI thread. Returns whether rows
   * lay out differently for it.
   */
  fun resolveDirection(rightToLeft: Boolean): Boolean {
    val previous = isRightToLeft
    resolvedRightToLeft = rightToLeft
    return isRightToLeft != previous
  }

  // endregion

  // region Window

  /*
   * The height of the window a list was last resolved in, or of the screen.
   */
  @Volatile
  var windowHeightPx: Int = 0
    get() = if (field > 0) field else Resources.getSystem().displayMetrics.heightPixels

  // endregion

  private const val LARGE_TEXT_SCALE = 1.3f
}

/*
 * Dp to pixels, rounded and exact. Theme tokens and template options are dp. Layouts are pixels.
 */
internal val Float.px: Int get() = (this * Resources.getSystem().displayMetrics.density).roundToInt()
internal val Float.pxf: Float get() = this * Resources.getSystem().displayMetrics.density

/*
 * Sp to pixels, which follows the reader's text size.
 */
internal val Float.spf: Float
  get() {
    val metrics = Resources.getSystem().displayMetrics
    val scale = ShadowListUtilsPlatform.textScaleOverride
    return if (scale != null) this * metrics.density * scale else TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, this, metrics)
  }

/*
 * Pixels back to dp, for the numbers the C++ layout helpers take.
 */
internal val Float.dp: Float get() = this / Resources.getSystem().displayMetrics.density

/*
 * Whether the app is a debug build, for checks a release build skips.
 */
internal val Context.isDebuggable: Boolean
  get() = (applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0

/*
 * A color with its alpha scaled by opacity, from 0 to 1.
 */
internal fun Int.withOpacity(opacity: Float): Int {
  val alpha = ((this ushr 24) * opacity).roundToInt().coerceIn(0, 255)
  return (this and 0x00FFFFFF) or (alpha shl 24)
}

/*
 * The frame of typeset text placed at a point.
 */
internal fun frameOf(left: Int, top: Int, layout: ShadowListKitTextLayout) =
  Rect(left, top, left + layout.width, top + layout.height)

/*
 * The frame at the same distance from the other side of a container of a width, for a layout
 * that runs from right to left.
 */
internal fun Rect.mirrored(widthPx: Int): Rect = Rect(widthPx - right, top, widthPx - left, bottom)

/*
 * Places the frames of a finished layout: as they are, or mirrored in the row's width when the
 * app lays out from right to left. The frame of a part a row lacks stays empty.
 */
internal class ShadowListUtilsDirection(private val widthPx: Int) {
  val isRightToLeft = ShadowListUtilsPlatform.isRightToLeft

  fun place(frame: Rect): Rect = place(frame, widthPx)

  /*
   * The same for frames inside a container of another width.
   */
  fun place(frame: Rect, widthPx: Int): Rect = if (isRightToLeft && !frame.isEmpty) frame.mirrored(widthPx) else frame
}

/*
 * Measure and place a child at a frame in one step. A child that only moved keeps its measure.
 */
internal fun View.place(left: Int, top: Int, width: Int, height: Int) {
  if (isLayoutRequested || measuredWidth != width || measuredHeight != height) {
    measure(
      View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY))
  }
  layout(left, top, left + width, top + height)
}

internal fun View.place(frame: Rect) = place(frame.left, frame.top, frame.width(), frame.height())

/*
 * Show a text layout at a frame in a color, or hide the view without one.
 */
internal fun ShadowListKitTextView.show(layout: ShadowListKitTextLayout?, color: Int, frame: Rect) {
  textColor = color
  textLayout = layout
  visibility = if (layout == null) View.GONE else View.VISIBLE
  if (layout != null) place(frame)
}

/*
 * Clips a view to its bounds with rounded corners. The radius is in pixels.
 */
internal class ShadowListUtilsRoundedOutline(var radius: Float) : ViewOutlineProvider() {
  override fun getOutline(view: View, outline: Outline) {
    outline.setRoundRect(0, 0, view.width, view.height, radius)
  }
}

/*
 * Clips a view to its bounds with only the bottom corners rounded, like a caption along the
 * bottom of a rounded card. The radius is in pixels.
 */
internal class ShadowListUtilsBottomRoundedOutline(var radius: Float) : ViewOutlineProvider() {
  override fun getOutline(view: View, outline: Outline) {
    // A rounded rect that starts a radius above the view rounds only what shows.
    outline.setRoundRect(0, -radius.toInt() - 1, view.width, view.height, radius)
  }
}

/*
 * A hint a screen reader says after the label. Before API 26 a node has no hint, and the hint
 * joins the label.
 */
internal fun AccessibilityNodeInfo.setHint(hint: String) {
  if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
    hintText = hint
  } else {
    contentDescription = listOfNotNull(contentDescription, hint).joinToString(", ")
  }
}

/*
 * The three icons the rows draw: the grip of a reorderable row, the chevron of a row that opens
 * something and the check of a selected row. Each fills its view, which is square.
 */
internal class ShadowListUtilsIconView(context: Context, private val kind: Kind) : View(context) {
  enum class Kind { GRIP, CHEVRON, CHECK }

  private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
  private val path = Path()

  /*
   * The direction the chevron was last drawn for.
   */
  private var pathRightToLeft = false

  var color: Int
    get() = paint.color
    set(value) {
      if (paint.color == value) return
      paint.color = value
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
    if (kind == Kind.CHEVRON && pathRightToLeft != ShadowListUtilsPlatform.isRightToLeft) buildPath()
    paint.style = Paint.Style.STROKE
    paint.strokeWidth = (if (kind == Kind.CHECK) CHECK_STROKE_DP else CHEVRON_STROKE_DP).pxf
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
        // Two arms of CHEVRON_ARM_SHARE of the size, meeting at a right angle.
        val reach = size * CHEVRON_ARM_SHARE * HALF_DIAGONAL
        val direction = if (pathRightToLeft) -1f else 1f
        path.moveTo(size / 2f - direction * reach / 2f, size / 2f - reach)
        path.lineTo(size / 2f + direction * reach / 2f, size / 2f)
        path.lineTo(size / 2f - direction * reach / 2f, size / 2f + reach)
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
    const val CHEVRON_ARM_SHARE = 0.42f
    const val CHECK_STROKE_DP = 2.2f
    const val CHECK_SHORT_SHARE = 0.26f
    const val CHECK_LONG_SHARE = 0.5f
    const val CHECK_LIFT_SHARE = 0.06f
  }
}
