package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration
import android.graphics.Rect
import android.os.Build
import android.view.ViewGroup
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView

/*
 * A large title with an optional subtitle, for a list's headerView. The list sizes it through
 * onMeasure.
 */
class ShadowListUtilsListHeader(
  context: Context,
  val title: String,
  val subtitle: String? = null,
  theme: ShadowListUtilsTheme,
) : ViewGroup(context) {
  var theme: ShadowListUtilsTheme = theme
    set(value) {
      field = value
      applyTheme()
    }

  private val titleView = ShadowListKitTextView(context)
  private val subtitleView = ShadowListKitTextView(context)
  private var titleLayout: ShadowListKitTextLayout? = null
  private var subtitleLayout: ShadowListKitTextLayout? = null
  private var layoutWidthPx = -1

  init {
    addView(titleView)
    addView(subtitleView)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) titleView.isAccessibilityHeading = true
    titleView.contentDescription = title
    subtitleView.contentDescription = subtitle
    applyTheme()
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    layoutText(width)
    var height = theme.spacing.xs.px + (titleLayout?.height ?: 0) + theme.spacing.md.px
    subtitleLayout?.let { height += theme.spacing.xxs.px + it.height }
    setMeasuredDimension(width, resolveSize(height, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    val width = right - left
    layoutText(width)
    val x = theme.spacing.lg.px
    var y = theme.spacing.xs.px
    titleView.show(titleLayout, theme.colors.label, leadingFrame(x, y, titleLayout, width))
    y += (titleLayout?.height ?: 0) + theme.spacing.xxs.px
    subtitleView.show(subtitleLayout, theme.colors.secondaryLabel, leadingFrame(x, y, subtitleLayout, width))
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
    titleLayout = theme.typography.largeTitle.layout(title, textWidth)
    subtitleLayout = subtitle?.takeIf { it.isNotEmpty() }?.let { theme.typography.subhead.layout(it, textWidth) }
  }

  /*
   * The frame of text at the leading edge, which is the right one for a right-to-left reader.
   */
  private fun leadingFrame(x: Int, y: Int, layout: ShadowListKitTextLayout?, width: Int): Rect {
    val frame = layout?.let { frameOf(x, y, it) } ?: Rect()
    return if (ShadowListUtilsPlatform.isRightToLeft) frame.mirrored(width) else frame
  }
}
