package com.shadowlist.utils

import android.text.Layout
import android.text.SpannableString
import android.text.Spanned
import android.text.StaticLayout
import android.text.TextDirectionHeuristics
import android.text.TextUtils
import android.text.style.TypefaceSpan
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.TextView

/*
 * One immutable typesetting configuration for background row measurement and native selectable
 * text. No View, selection state or mutable editor escapes to the prefetch thread.
 */
internal class ShadowListUtilsAssistantDisclosureTextLayout(
  text: String, widthPx: Int, style: ShadowListUtilsTextStyle, expanded: Boolean, monospaced: Boolean = false,
) {
  private val lineHeightPx = style.scaledLineHeightPx()
  val maximumLines = if (expanded) Int.MAX_VALUE else 1
  val selectable = expanded
  val width = widthPx.coerceAtLeast(1)
  val layout: StaticLayout
  val height: Int get() = layout.height

  init {
    val runs = SpannableString(text)
    if (monospaced) runs.setSpan(TypefaceSpan("monospace"), 0, runs.length, Spanned.SPAN_INCLUSIVE_INCLUSIVE)
    val base = style.layout(runs, width, if (expanded) 0 else 1, scaled = true)
    val content = base.layout.text
    layout = StaticLayout.Builder.obtain(content, 0, content.length, base.layout.paint, width)
      .setAlignment(Layout.Alignment.ALIGN_NORMAL).setIncludePad(false).setLineSpacing(0f, 1f)
      .setBreakStrategy(Layout.BREAK_STRATEGY_SIMPLE).setHyphenationFrequency(Layout.HYPHENATION_FREQUENCY_NONE)
      .setTextDirection(TextDirectionHeuristics.FIRSTSTRONG_LTR)
      .setMaxLines(maximumLines).setEllipsize(if (expanded) null else TextUtils.TruncateAt.START).build()
  }

  fun apply(view: ShadowListUtilsAssistantDisclosureTextView, color: Int) {
    view.setPadding(0, 0, 0, 0)
    view.includeFontPadding = false
    view.gravity = Gravity.TOP or Gravity.START
    view.breakStrategy = Layout.BREAK_STRATEGY_SIMPLE
    view.hyphenationFrequency = Layout.HYPHENATION_FREQUENCY_NONE
    view.textDirection = View.TEXT_DIRECTION_FIRST_STRONG_LTR
    view.setLineSpacing(0f, 1f)
    view.maxLines = maximumLines
    view.ellipsize = if (selectable) null else TextUtils.TruncateAt.START
    view.setTextSize(TypedValue.COMPLEX_UNIT_PX, layout.paint.textSize)
    view.typeface = layout.paint.typeface
    view.letterSpacing = layout.paint.letterSpacing
    view.setTextColor(color)
    view.setTextIsSelectable(selectable)
    view.linksClickable = false
    view.autoLinkMask = 0
    val previous = view.preparedLayout
    val sameRuns = previous != null && previous.layout.text.toString() == layout.text.toString() &&
      previous.lineHeightPx == lineHeightPx && previous.maximumLines == maximumLines &&
      previous.layout.paint.textSize == layout.paint.textSize && previous.layout.paint.typeface == layout.paint.typeface &&
      previous.layout.paint.letterSpacing == layout.paint.letterSpacing &&
      (previous.layout.text as Spanned).getSpans(0, previous.layout.text.length, TypefaceSpan::class.java).map { it.family } ==
      (layout.text as Spanned).getSpans(0, layout.text.length, TypefaceSpan::class.java).map { it.family }
    if (!sameRuns) {
      val sameText = view.text.toString() == layout.text.toString()
      val start = view.selectionStart
      val end = view.selectionEnd
      view.text = layout.text
      if (sameText && selectable && start >= 0 && end >= start && end <= view.text.length) {
        android.text.Selection.setSelection(view.text as android.text.Spannable, start, end)
      }
    }
    view.preparedLayout = this
  }
}
