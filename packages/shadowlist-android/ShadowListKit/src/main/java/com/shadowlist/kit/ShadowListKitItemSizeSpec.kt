package com.shadowlist.kit

import android.graphics.Typeface
import android.os.Build
import android.text.TextPaint
import android.text.SpannableString
import android.text.Spanned
import android.text.style.LineHeightSpan
import androidx.annotation.Keep
import kotlin.math.ceil

/* Lengths are logical dp, matching the native row's public text and inset dimensions. */
@Keep
data class ShadowListKitItemSizeSpec(
  val text: String,
  val fontSize: Double = 14.0,
  val fontFamily: String? = null,
  val fontWeight: String? = null,
  val fontStyle: String? = null,
  val lineHeight: Double = Double.NaN,
  val letterSpacing: Double = Double.NaN,
  val numberOfLines: Int = 0,
  val insetWidth: Double = 0.0,
  val insetHeight: Double = 0.0,
  val widthFraction: Double = 1.0,
  val fixedHeight: Double = Double.NaN,
) {
  internal fun pixels(scale: Double) = copy(fontSize = fontSize * scale, lineHeight = lineHeight * scale,
    letterSpacing = letterSpacing * scale, insetWidth = insetWidth * scale, insetHeight = insetHeight * scale,
    fixedHeight = fixedHeight * scale)

  internal fun measure(width: Double): Double {
    if (!fixedHeight.isNaN()) return fixedHeight
    val fraction = if (widthFraction > 0 && widthFraction <= 1) widthFraction else 1.0
    val textWidth = width * fraction - insetWidth
    if (textWidth <= 0) return insetHeight
    val paint = TextPaint(TextPaint.ANTI_ALIAS_FLAG)
    paint.textSize = (if (fontSize.isFinite() && fontSize > 0) fontSize else 14.0).toFloat()
    val weight = when (fontWeight) { "bold" -> 700; "normal", null -> 400; else -> fontWeight.toIntOrNull() ?: 400 }
    val italic = fontStyle == "italic"
    val family = Typeface.create(fontFamily, Typeface.NORMAL)
    paint.typeface = if (Build.VERSION.SDK_INT >= 28) Typeface.create(family, weight.coerceIn(1, 1000), italic)
      else Typeface.create(family, when {
        weight >= 600 && italic -> Typeface.BOLD_ITALIC
        weight >= 600 -> Typeface.BOLD
        italic -> Typeface.ITALIC
        else -> Typeface.NORMAL
      })
    if (letterSpacing.isFinite()) paint.letterSpacing = (letterSpacing / paint.textSize).toFloat()
    val content = if (lineHeight.isFinite() && lineHeight > 0 && text.isNotEmpty()) {
      SpannableString(text).apply {
        val height = ceil(lineHeight).toInt()
        setSpan(LineHeightSpan { _, _, _, _, _, metrics ->
          val extra = height - (metrics.descent - metrics.ascent)
          metrics.descent += extra / 2
          metrics.ascent = metrics.descent - height
          metrics.top = metrics.ascent
          metrics.bottom = metrics.descent
        }, 0, length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
      }
    } else text
    val layout = ShadowListKitTextLayout.make(content, paint, ceil(textWidth).toInt(),
      if (numberOfLines > 0) numberOfLines else Int.MAX_VALUE)
    return layout.height + insetHeight
  }
}
