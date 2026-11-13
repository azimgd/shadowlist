package com.shadowlist.utils

import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.text.SpannableString
import android.text.Spanned
import android.text.style.StyleSpan
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView

/* Empty overlay, intentionally outside the transcript's row/content-size model. */
class ShadowListUtilsAssistantEmpty(context: Context, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : ViewGroup(context) {
  var suggestions = emptyList<ShadowListUtilsAssistantSuggestion>()
    set(value) { field = value.toList(); rebuildSuggestions() }
  var onSelectSuggestion: ((ShadowListUtilsAssistantSuggestion) -> Unit)? = null
    set(value) { field = value; buttons.forEach { it.isEnabled = value != null } }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; requestLayout() }
  private val mark = TextView(context).apply {
    text = "✦"; textSize = 28f; gravity = Gravity.CENTER; setTextColor(theme.colors.onAccent)
    background = GradientDrawable().apply { setColor(theme.colors.accent); cornerRadius = 28f.pxf }
  }
  private val title = TextView(context).apply { textSize = theme.typography.title2.fontSize; setTextColor(theme.colors.label); gravity = Gravity.CENTER; setTypeface(typeface, Typeface.BOLD); if (android.os.Build.VERSION.SDK_INT >= 28) isAccessibilityHeading = true }
  private val subtitle = TextView(context).apply { textSize = theme.typography.subhead.fontSize; setTextColor(theme.colors.secondaryLabel); gravity = Gravity.CENTER }
  private val buttons = mutableListOf<Button>()
  init { addView(mark); addView(title); addView(subtitle) }
  private fun rebuildSuggestions() {
    buttons.forEach(::removeView); buttons.clear()
    for (suggestion in suggestions) {
      val button = Button(context).apply {
        isAllCaps = false; gravity = Gravity.START or Gravity.CENTER_VERTICAL; textSize = theme.typography.footnote.fontSize
        text = SpannableString(suggestion.title + "\n" + suggestion.prompt).apply { setSpan(StyleSpan(Typeface.BOLD), 0, suggestion.title.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE) }
        setTextColor(theme.colors.label); setPadding(theme.spacing.md.px, theme.spacing.md.px, theme.spacing.md.px, theme.spacing.md.px)
        background = GradientDrawable().apply { setColor(theme.colors.elevated); cornerRadius = theme.radius.md.pxf }
        contentDescription = suggestion.title + ". " + suggestion.prompt
        isEnabled = onSelectSuggestion != null
        setOnClickListener { onSelectSuggestion?.invoke(suggestion) }
      }
      buttons.add(button); addView(button)
    }
    requestLayout()
  }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    val inner = (width - 2 * theme.spacing.lg.px).coerceAtLeast(1)
    title.text = labels.emptyTitle; subtitle.text = labels.emptySubtitle
    title.measure(MeasureSpec.makeMeasureSpec(inner, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
    subtitle.measure(MeasureSpec.makeMeasureSpec(inner, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
    val columns = if (resources.configuration.fontScale > 1.4f || inner < 240f.px) 1 else 2
    val cardWidth = (inner - (columns - 1) * theme.spacing.sm.px) / columns
    for (button in buttons) button.measure(MeasureSpec.makeMeasureSpec(cardWidth, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
    var height = 48f.px + 56f.px + theme.spacing.lg.px + title.measuredHeight
    if (labels.emptySubtitle.isNotEmpty()) height += theme.spacing.xs.px + subtitle.measuredHeight
    if (buttons.isNotEmpty()) {
      height += theme.spacing.xxl.px
      for (row in buttons.chunked(columns)) height += row.maxOf { it.measuredHeight } + theme.spacing.sm.px
      height -= theme.spacing.sm.px
    }
    setMeasuredDimension(width, resolveSize(height + theme.spacing.xxl.px, heightMeasureSpec))
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    val spacing = theme.spacing; val inset = spacing.lg.px
    var y = 48f.px
    mark.layout((width - 56f.px) / 2, y, (width + 56f.px) / 2, y + 56f.px)
    y += 56f.px + spacing.lg.px
    title.layout(inset, y, width - inset, y + title.measuredHeight); y += title.measuredHeight
    subtitle.visibility = if (labels.emptySubtitle.isEmpty()) GONE else VISIBLE
    if (labels.emptySubtitle.isNotEmpty()) { y += spacing.xs.px; subtitle.layout(inset, y, width - inset, y + subtitle.measuredHeight); y += subtitle.measuredHeight }
    if (buttons.isEmpty()) return
    y += spacing.xxl.px
    val columns = if (resources.configuration.fontScale > 1.4f || width - 2 * inset < 240f.px) 1 else 2
    for (row in buttons.chunked(columns)) {
      val height = row.maxOf { it.measuredHeight }
      row.forEachIndexed { index, button ->
        val x = inset + index * (button.measuredWidth + spacing.sm.px)
        val start = if (layoutDirection == LAYOUT_DIRECTION_RTL) width - x - button.measuredWidth else x
        button.layout(start, y, start + button.measuredWidth, y + height)
      }
      y += height + spacing.sm.px
    }
  }
}
