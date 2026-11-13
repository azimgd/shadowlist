package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration
import android.view.ViewGroup
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView

/*
 * A centered line of secondary text, for a list's footerView. The list sizes it through
 * onMeasure.
 */
class ShadowListUtilsListFooter(context: Context, val text: String, theme: ShadowListUtilsTheme) : ViewGroup(context) {
  var theme: ShadowListUtilsTheme = theme
    set(value) {
      field = value
      applyTheme()
    }

  private val label = ShadowListKitTextView(context)
  private var textLayout: ShadowListKitTextLayout? = null
  private var layoutWidthPx = -1

  init {
    addView(label)
    label.contentDescription = text
    applyTheme()
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    layoutText(width)
    val height = 2 * theme.spacing.xl.px + (textLayout?.height ?: 0)
    setMeasuredDimension(width, resolveSize(height, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    val width = right - left
    layoutText(width)
    val layout = textLayout ?: return
    label.textColor = theme.colors.secondaryLabel
    label.textLayout = layout
    // Centered lines are placed inside the width they were typeset for.
    label.place(theme.spacing.lg.px, theme.spacing.xl.px, (width - 2 * theme.spacing.lg.px).coerceAtLeast(1), layout.height)
  }

  private fun applyTheme() {
    setBackgroundColor(theme.colors.background)
    layoutWidthPx = -1
    requestLayout()
  }

  /*
   * A new text size or layout direction typesets the text again.
   */
  override fun onConfigurationChanged(newConfig: Configuration) {
    super.onConfigurationChanged(newConfig)
    layoutWidthPx = -1
    requestLayout()
  }

  private fun layoutText(widthPx: Int) {
    if (widthPx == layoutWidthPx) return
    layoutWidthPx = widthPx
    val textWidth = (widthPx - 2 * theme.spacing.lg.px).coerceAtLeast(1)
    textLayout = theme.typography.footnote.layout(text, textWidth, 0, scaled = true, centered = true)
  }
}
