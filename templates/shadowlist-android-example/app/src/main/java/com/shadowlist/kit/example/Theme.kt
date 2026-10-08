package com.shadowlist.kit.example

import android.content.res.Resources
import android.graphics.Typeface
import android.text.TextPaint
import com.shadowlist.kit.SLKTextLayout
import kotlin.math.ceil
import kotlin.math.roundToInt

private val density = Resources.getSystem().displayMetrics.density

/*
 * Design points to pixels.
 */
val Number.dp: Int get() = (toFloat() * density).roundToInt()
val Number.dpf: Float get() = toFloat() * density

/*
 * The example's design tokens, the light values of shadowlist-utils' theme and the UIKit example.
 */
object Theme {
  const val background = 0xFFFFFFFF.toInt()
  const val elevated = 0xFFF2F2F7.toInt()
  const val elevated2 = 0xFFE5E5EA.toInt()
  const val label = 0xFF000000.toInt()
  const val secondaryLabel = 0x993C3C43.toInt()
  const val tertiaryLabel = 0x4D3C3C43.toInt()
  const val separator = 0x4A3C3C43
  const val accent = 0xFF007AFF.toInt()
  const val onAccent = 0xFFFFFFFF.toInt()
  const val onAccentSecondary = 0xA6FFFFFF.toInt()

  val avatarPalette = intArrayOf(
    0xFFFF6B6B.toInt(), 0xFF4ECDC4.toInt(), 0xFF45B7D1.toInt(), 0xFFFFA07A.toInt(), 0xFF98D8C8.toInt(),
    0xFFF7DC6F.toInt(), 0xFFBB8FCE.toInt(), 0xFF85C1E2.toInt(), 0xFFF8B195.toInt(), 0xFFC06C84.toInt(),
  )

  val hairline: Int get() = 1
}

/*
 * A text style: size, weight and line height. Layouts made with it are safe to build on any thread.
 */
class TextStyle(size: Float, semibold: Boolean, lineHeight: Float) {
  val paint = TextPaint(TextPaint.ANTI_ALIAS_FLAG).apply {
    textSize = size.dpf
    typeface = if (semibold) Typeface.create("sans-serif-medium", Typeface.NORMAL) else Typeface.DEFAULT
  }
  private val lineSpacingExtra = lineHeight.dpf - (paint.fontMetrics.descent - paint.fontMetrics.ascent)
  val lineHeightPx: Int = ceil(lineHeight.dpf).toInt()

  fun layout(text: String, width: Int, maxLines: Int = Int.MAX_VALUE): SLKTextLayout =
    SLKTextLayout.make(text, paint, width, maxLines, lineSpacingExtra.coerceAtLeast(0f))

  companion object {
    val largeTitle = TextStyle(34f, true, 41f)
    val body = TextStyle(17f, false, 22f)
    val subhead = TextStyle(15f, false, 20f)
    val subheadSemibold = TextStyle(15f, true, 20f)
    val footnote = TextStyle(13f, false, 18f)
    val footnoteSemibold = TextStyle(13f, true, 18f)
    val caption = TextStyle(12f, false, 16f)
  }
}
