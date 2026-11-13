package com.shadowlist.utils

import android.content.res.Resources
import android.graphics.Paint
import android.graphics.Typeface
import android.os.Build
import android.text.Layout
import android.text.SpannableString
import android.text.Spanned
import android.text.TextPaint
import android.text.style.AlignmentSpan
import android.text.style.LineHeightSpan
import com.shadowlist.kit.ShadowListKitTextLayout
import java.util.concurrent.ConcurrentHashMap
import kotlin.math.ceil

enum class ShadowListUtilsFontWeight { REGULAR, SEMIBOLD, BOLD }

/*
 * A text style: size, weight, line height and letter spacing of the system font, in dp at the
 * system's default text size. Text follows the reader's text size setting. Measuring and
 * drawing use the same typeset lines. A height computed off the UI thread is the height the
 * text shows at.
 */
data class ShadowListUtilsTextStyle(
  val fontSize: Float,
  val fontWeight: ShadowListUtilsFontWeight,
  val lineHeight: Float,
  val letterSpacing: Float,
) {
  fun with(fontWeight: ShadowListUtilsFontWeight): ShadowListUtilsTextStyle = copy(fontWeight = fontWeight)

  /* Apply native widget metrics on its UI thread; the widget measures and paints the same text. */
  internal fun applyTo(view: android.widget.TextView, @androidx.annotation.ColorInt color: Int, weight: Int = fontWeight.weight) {
    view.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, fontSize)
    view.typeface = typeface(weight)
    view.letterSpacing = if (fontSize > 0f) letterSpacing / fontSize else 0f
    val height = android.util.TypedValue.applyDimension(android.util.TypedValue.COMPLEX_UNIT_SP, lineHeight, view.resources.displayMetrics)
    androidx.core.widget.TextViewCompat.setLineHeight(view, ceil(height).toInt())
    view.setTextColor(color)
  }

  /*
   * The text typeset at a width in pixels for a ShadowListKitTextView. maxLines 0 means no
   * limit. Every line is the style's line height tall. Thread safe, row layouts call it off the
   * UI thread.
   */
  fun layout(text: String, widthPx: Int, maxLines: Int = 0): ShadowListKitTextLayout =
    layout(text, widthPx, maxLines, scaled = true)

  /*
   * The same with the switches the templates use: scaled off keeps the text at its size
   * whatever the text size setting, like initials in an avatar, centered places every line in
   * the middle of widthPx, and weight is a font weight the public weights do not name.
   */
  internal fun layout(
    text: CharSequence, widthPx: Int, maxLines: Int, scaled: Boolean, centered: Boolean = false, weight: Int? = null,
  ): ShadowListKitTextLayout {
    val spanned = SpannableString(text)
    spanned.setSpan(LineHeight(scaledLineHeightPx(scaled)), 0, text.length, Spanned.SPAN_INCLUSIVE_INCLUSIVE)
    if (centered) {
      spanned.setSpan(AlignmentSpan.Standard(Layout.Alignment.ALIGN_CENTER), 0, text.length, Spanned.SPAN_INCLUSIVE_INCLUSIVE)
    }
    val paint = paint(scaled, weight ?: fontWeight.weight)
    return ShadowListKitTextLayout.make(spanned, paint, widthPx, if (maxLines <= 0) Int.MAX_VALUE else maxLines)
  }

  /*
   * The height of one line as text is laid out with it: the style's at the reader's text size,
   * rounded up to the pixel grid.
   */
  internal fun scaledLineHeightPx(scaled: Boolean = true): Int = ceil(if (scaled) lineHeight.spf else lineHeight.pxf).toInt()

  /*
   * The paint of the style, built once per size, weight and letter spacing. One paint is shared
   * by the layouts of every thread: laying out reads it and only drawing, on the UI thread, sets its color.
   */
  private fun paint(scaled: Boolean, weight: Int): TextPaint {
    val fontPx = if (scaled) fontSize.spf else fontSize.pxf
    val density = Resources.getSystem().displayMetrics.density
    val key = PaintKey(fontPx, weight, if (fontSize > 0f) letterSpacing / fontSize else 0f, density)
    return paints.getOrPut(key) {
      TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        textSize = key.fontPx
        typeface = typeface(key.weight)
        // The style's spacing is in dp at its font size, which is the same spacing in em.
        letterSpacing = key.letterSpacingEm
      }
    }
  }

  private data class PaintKey(
    val fontPx: Float,
    val weight: Int,
    val letterSpacingEm: Float,
    val density: Float,
  )

  /*
   * Gives every line the same height, with the font's own height centered in it.
   */
  private class LineHeight(private val heightPx: Int) : LineHeightSpan {
    override fun chooseHeight(
      text: CharSequence, start: Int, end: Int, spanstartv: Int, lineHeight: Int, metrics: Paint.FontMetricsInt,
    ) {
      val extra = heightPx - (metrics.descent - metrics.ascent)
      val above = Math.floorDiv(extra + 1, 2)
      metrics.ascent -= above
      metrics.descent += extra - above
      metrics.top = metrics.ascent
      metrics.bottom = metrics.descent
    }
  }

  internal companion object {
    /*
     * The weights of the system font the styles are set in. Medium is not a public weight: the
     * empty state's title takes it.
     */
    const val WEIGHT_REGULAR = 400
    const val WEIGHT_MEDIUM = 500
    const val WEIGHT_SEMIBOLD = 600
    const val WEIGHT_BOLD = 700

    private val paints = ConcurrentHashMap<PaintKey, TextPaint>()

    private val ShadowListUtilsFontWeight.weight: Int
      get() = when (this) {
        ShadowListUtilsFontWeight.REGULAR -> WEIGHT_REGULAR
        ShadowListUtilsFontWeight.SEMIBOLD -> WEIGHT_SEMIBOLD
        ShadowListUtilsFontWeight.BOLD -> WEIGHT_BOLD
      }

    /*
     * The system font at a weight. Before API 28 a typeface has no numeric weight, and the
     * weights between regular and bold take the system's medium face.
     */
    internal fun typeface(weight: Int): Typeface = when {
      weight <= WEIGHT_REGULAR -> Typeface.DEFAULT
      weight >= WEIGHT_BOLD -> Typeface.DEFAULT_BOLD
      Build.VERSION.SDK_INT >= Build.VERSION_CODES.P -> Typeface.create(Typeface.DEFAULT, weight, false)
      else -> Typeface.create("sans-serif-medium", Typeface.NORMAL)
    }
  }
}
