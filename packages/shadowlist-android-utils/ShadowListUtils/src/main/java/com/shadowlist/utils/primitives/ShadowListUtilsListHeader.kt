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
    // The vertical padding rounds once, the way React Native rounds the container.
    val gap = if (subtitleLayout != null) theme.spacing.xxs else 0f
    val height = (theme.spacing.xs + theme.spacing.md + gap).px + (titleLayout?.height ?: 0) + (subtitleLayout?.height ?: 0)
    setMeasuredDimension(width, resolveSize(height, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    val width = right - left
    layoutText(width)
    val x = theme.spacing.lg.px
    // React Native floors a text origin to the pixel grid.
    val y = kotlin.math.floor(theme.spacing.xs.pxf).toInt()
    titleView.show(titleLayout, theme.colors.label, leadingFrame(x, y, titleLayout, width))
    val subtitleY = kotlin.math.floor((theme.spacing.xs + theme.spacing.xxs).pxf).toInt() + (titleLayout?.height ?: 0)
    subtitleView.show(subtitleLayout, theme.colors.secondaryLabel, leadingFrame(x, subtitleY, subtitleLayout, width))
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
