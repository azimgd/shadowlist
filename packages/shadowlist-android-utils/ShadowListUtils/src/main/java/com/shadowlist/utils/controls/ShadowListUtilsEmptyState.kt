package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration
import android.view.View
import android.view.ViewGroup
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView

/*
 * What a screen says before it has anything to show: an optional icon, a title and an optional
 * message in a centered column. It is read as one accessibility element.
 */
class ShadowListUtilsEmptyState(
  context: Context,
  val title: String,
  val message: String? = null,
  val icon: View? = null,
  theme: ShadowListUtilsTheme,
) : ViewGroup(context) {
  var theme: ShadowListUtilsTheme = theme
    set(value) {
      field = value
      layoutWidthPx = -1
      requestLayout()
    }

  private val titleView = ShadowListKitTextView(context)
  private val messageView = ShadowListKitTextView(context)
  private var titleLayout: ShadowListKitTextLayout? = null
  private var messageLayout: ShadowListKitTextLayout? = null
  private var layoutWidthPx = -1

  init {
    if (icon != null) addView(icon)
    addView(titleView)
    addView(messageView)
    contentDescription = if (message == null) title else "$title. $message"
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES
    for (child in listOf(icon, titleView, messageView)) child?.importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    layoutText(width)
    measureIcon()
    val gap = theme.spacing.xs.px
    var height = 2 * PADDING_VERTICAL_DP.px + TITLE_MARGIN_TOP_DP.px + (titleLayout?.height ?: 0)
    icon?.let { height += it.measuredHeight + gap }
    messageLayout?.let { height += gap + it.height }
    setMeasuredDimension(width, resolveSize(height, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    val width = right - left
    layoutText(width)
    val gap = theme.spacing.xs.px
    val x = theme.grouped.inset.px
    val textWidth = textWidth(width)
    // The column sits at the top of a frame taller than it needs.
    var y = PADDING_VERTICAL_DP.px
    icon?.let {
      it.layout((width - it.measuredWidth) / 2, y, (width + it.measuredWidth) / 2, y + it.measuredHeight)
      y += it.measuredHeight + gap
    }
    y += TITLE_MARGIN_TOP_DP.px
    titleLayout?.let {
      titleView.textColor = theme.colors.label
      titleView.textLayout = it
      // Centered lines are placed inside the width they were typeset for.
      titleView.place(x, y, textWidth, it.height)
      y += it.height + gap
    }
    val message = messageLayout
    messageView.visibility = if (message == null) GONE else VISIBLE
    if (message != null) {
      messageView.textColor = theme.colors.secondaryLabel
      messageView.textLayout = message
      messageView.place(x, y, textWidth, message.height)
    }
  }

  /*
   * A new text size or layout direction typesets the text again.
   */
  override fun onConfigurationChanged(newConfig: Configuration) {
    super.onConfigurationChanged(newConfig)
    layoutWidthPx = -1
    requestLayout()
  }

  private fun textWidth(widthPx: Int): Int = (widthPx - 2 * theme.grouped.inset.px).coerceAtLeast(1)

  private fun layoutText(widthPx: Int) {
    if (widthPx == layoutWidthPx) return
    layoutWidthPx = widthPx
    // The title is the body style at medium weight, which the public weights do not name.
    titleLayout = theme.typography.body.layout(
      title, textWidth(widthPx), 0, scaled = true, centered = true, weight = ShadowListUtilsTextStyle.WEIGHT_MEDIUM)
    messageLayout = message?.let { theme.typography.footnote.layout(it, textWidth(widthPx), 0, scaled = true, centered = true) }
  }

  private fun measureIcon() {
    val icon = icon ?: return
    val params = icon.layoutParams
    icon.measure(childSpec(params?.width ?: LayoutParams.WRAP_CONTENT), childSpec(params?.height ?: LayoutParams.WRAP_CONTENT))
  }

  private fun childSpec(size: Int): Int =
    if (size >= 0) MeasureSpec.makeMeasureSpec(size, MeasureSpec.EXACTLY) else MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED)

  private companion object {
    const val PADDING_VERTICAL_DP = 28f
    const val TITLE_MARGIN_TOP_DP = 6f
  }
}
