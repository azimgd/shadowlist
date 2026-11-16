package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import kotlin.math.floor
import kotlin.math.roundToInt

/*
 * An empty-state overlay; it never contributes rows to the transcript's content size.
 */
class ShadowListUtilsAssistantEmpty(context: Context, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : ViewGroup(context) {
  var suggestions = emptyList<ShadowListUtilsAssistantSuggestion>()
    set(value) { field = value.toList(); rebuildSuggestions() }
  var onSelectSuggestion: ((ShadowListUtilsAssistantSuggestion) -> Unit)? = null
    set(value) { field = value; buttons.forEach { it.isEnabled = value != null } }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; requestLayout() }
  private val mark = TextView(context).apply {
    foreground = ShadowListUtilsSparkleIcon(theme.colors.onAccent, 28f)
    background = GradientDrawable().apply { setColor(theme.colors.accent); cornerRadius = 28f.pxf }
  }
  private val title = TextView(context).apply { gravity = Gravity.CENTER; if (android.os.Build.VERSION.SDK_INT >= 28) isAccessibilityHeading = true }
  private val subtitle = TextView(context).apply { gravity = Gravity.CENTER }
  private val buttons = mutableListOf<LinearLayout>()
  private val frames = LinkedHashMap<View, Rect>()
  init { addView(mark); addView(title); addView(subtitle) }
  private fun rebuildSuggestions() {
    buttons.forEach(::removeView); buttons.clear()
    for (suggestion in suggestions) {
      val button = object : LinearLayout(context) {
        override fun onInitializeAccessibilityNodeInfo(info: android.view.accessibility.AccessibilityNodeInfo) {
          super.onInitializeAccessibilityNodeInfo(info)
          info.className = android.widget.Button::class.java.name
        }
      }.apply {
        orientation = LinearLayout.VERTICAL
        val cardTitle = TextView(context).apply {
          importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
          text = suggestion.title
          theme.typography.subhead.with(ShadowListUtilsFontWeight.SEMIBOLD).applyTo(this, theme.colors.label)
        }
        val cardPrompt = TextView(context).apply {
          importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
          text = suggestion.prompt
          maxLines = 2
          ellipsize = android.text.TextUtils.TruncateAt.END
          theme.typography.footnote.applyTo(this, theme.colors.secondaryLabel)
        }
        addView(cardTitle, LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
        // React Native floors a text origin to the pixel grid and rounds the card's insets once.
        val inset = floor(theme.spacing.md.pxf).toInt()
        val promptTop = floor((theme.spacing.md + theme.spacing.xxs).pxf).toInt() - inset
        addView(cardPrompt, LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT).apply { topMargin = promptTop })
        setPadding(inset, inset, (2 * theme.spacing.md).px - inset, cardInsets.roundToInt() - inset - promptTop)
        background = GradientDrawable().apply { setColor(theme.colors.elevated); cornerRadius = theme.radius.md.pxf }
        contentDescription = suggestion.title + ". " + suggestion.prompt
        importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES
        isEnabled = onSelectSuggestion != null
        setOnClickListener { onSelectSuggestion?.invoke(suggestion) }
      }
      buttons.add(button); addView(button)
    }
    requestLayout()
  }
  /*
   * What a card adds to its two texts, before rounding.
   */
  private val cardInsets: Float get() = (2 * theme.spacing.md + theme.spacing.xxs).pxf

  /*
   * Measures every child and keeps its frame. Offsets add up unrounded and each frame is rounded
   * once, the way React Native places views, with a text's origin floored. Returns the height.
   */
  private fun arrange(width: Int): Int {
    val spacing = theme.spacing
    val inset = spacing.lg.px
    val inner = (width - 2 * inset).coerceAtLeast(1)
    val markSize = MARK_SIZE_DP.px
    title.text = labels.emptyTitle; subtitle.text = labels.emptySubtitle
    theme.typography.title2.applyTo(title, theme.colors.label)
    theme.typography.subhead.applyTo(subtitle, theme.colors.secondaryLabel)
    val exactInner = MeasureSpec.makeMeasureSpec(inner, MeasureSpec.EXACTLY)
    val unbounded = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED)
    title.measure(exactInner, unbounded)
    subtitle.measure(exactInner, unbounded)
    frames.clear()
    var y = PADDING_TOP_DP.pxf
    val markLeft = ((width - markSize) / 2f).roundToInt()
    frames[mark] = Rect(markLeft, y.roundToInt(), markLeft + markSize, y.roundToInt() + markSize)
    y += (MARK_SIZE_DP + spacing.lg).pxf
    frames[title] = Rect(inset, floor(y).toInt(), width - inset, floor(y).toInt() + title.measuredHeight)
    y += title.measuredHeight
    subtitle.visibility = if (labels.emptySubtitle.isEmpty()) GONE else VISIBLE
    if (labels.emptySubtitle.isNotEmpty()) {
      y += spacing.xs.pxf
      frames[subtitle] = Rect(inset, floor(y).toInt(), width - inset, floor(y).toInt() + subtitle.measuredHeight)
      y += subtitle.measuredHeight
    }
    if (buttons.isNotEmpty()) {
      y += spacing.xxl.pxf
      val gridTop = y.roundToInt()
      val columns = if (resources.configuration.fontScale > 1.4f || inner < 240f.px) 1 else 2
      val cardWidth = (inner - (columns - 1) * spacing.sm.pxf) / columns
      var rowTop = 0f
      for (row in buttons.chunked(columns)) {
        val lefts = row.indices.map { (it * (cardWidth + spacing.sm.pxf)).roundToInt() }
        val rights = row.indices.map { (it * (cardWidth + spacing.sm.pxf) + cardWidth).roundToInt() }
        row.forEachIndexed { index, button ->
          button.measure(MeasureSpec.makeMeasureSpec(rights[index] - lefts[index], MeasureSpec.EXACTLY), unbounded)
        }
        val height = row.maxOf { it.measuredHeight } - cardInsets.roundToInt() + cardInsets
        val top = gridTop + rowTop.roundToInt()
        val bottom = gridTop + (rowTop + height).roundToInt()
        row.forEachIndexed { index, button ->
          val start = if (layoutDirection == LAYOUT_DIRECTION_RTL) inner - rights[index] else lefts[index]
          frames[button] = Rect(inset + start, top, inset + start + rights[index] - lefts[index], bottom)
        }
        rowTop += height + spacing.sm.pxf
      }
      y += rowTop - spacing.sm.pxf
    }
    return (y + spacing.xxl.pxf).roundToInt()
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    val height = arrange(width)
    for ((view, frame) in frames) {
      view.measure(MeasureSpec.makeMeasureSpec(frame.width(), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(frame.height(), MeasureSpec.EXACTLY))
    }
    setMeasuredDimension(width, resolveSize(height, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    for ((view, frame) in frames) view.layout(frame.left, frame.top, frame.right, frame.bottom)
  }

  private companion object {
    const val PADDING_TOP_DP = 48f
    const val MARK_SIZE_DP = 56f
  }
}
