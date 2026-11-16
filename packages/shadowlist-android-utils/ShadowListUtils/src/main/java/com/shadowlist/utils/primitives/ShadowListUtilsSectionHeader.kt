package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.os.Build
import android.view.accessibility.AccessibilityNodeInfo
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView

/*
 * The header of a section: a small title with an optional count at the trailing edge. The
 * title is upper cased when the theme's grouped caption case says so.
 */
data class ShadowListUtilsSectionHeader(
  override val key: String,
  val title: String,
  val count: Int? = null,
  val theme: ShadowListUtilsTheme,
) : ShadowListUtilsRow {
  override val contentVersion: Int = 31 * title.hashCode() + (count ?: -1)
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsSectionHeaderView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val footnote = theme.typography.footnote
    val inset = theme.spacing.lg.px
    val contentWidth = (widthPx - 2 * inset).coerceAtLeast(1)
    val direction = ShadowListUtilsDirection(widthPx)
    val countText = count?.let { footnote.layout(it.toString(), contentWidth, 1) }
    val countWidth = countText?.let { it.width + theme.spacing.sm.px } ?: 0
    val shown = if (theme.grouped.captionCase == ShadowListUtilsCaptionCase.UPPER) title.uppercase() else title
    val titleText = footnote.with(ShadowListUtilsFontWeight.SEMIBOLD).layout(shown, (contentWidth - countWidth).coerceAtLeast(1), 1)
    // React Native floors a text origin to the pixel grid and rounds the vertical padding once.
    val top = kotlin.math.floor(PADDING_VERTICAL_DP.pxf).toInt()
    val titleFrame = frameOf(inset, top, titleText)
    val countFrame = countText?.let { frameOf(widthPx - inset - it.width, top, it) } ?: Rect()
    return ShadowListUtilsSectionHeaderLayout(
      widthPx, footnote.scaledLineHeightPx() + (2 * PADDING_VERTICAL_DP).px, titleText, direction.place(titleFrame), countText, direction.place(countFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsSectionHeader"
    private const val PADDING_VERTICAL_DP = 10f
  }
}

class ShadowListUtilsSectionHeaderLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val title: ShadowListKitTextLayout,
  val titleFrame: Rect,
  val count: ShadowListKitTextLayout?,
  val countFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsSectionHeaderView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsSectionHeader.REUSE_IDENTIFIER) {
  private val title = ShadowListKitTextView(context)
  private val count = ShadowListKitTextView(context)

  init {
    addView(title)
    addView(count)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val header = row as ShadowListUtilsSectionHeader
    restingBackgroundColor = header.theme.colors.elevated
    contentDescription = header.count?.let { "${header.title}, $it" } ?: header.title
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val header = row as ShadowListUtilsSectionHeader
    val frames = layout as ShadowListUtilsSectionHeaderLayout
    title.show(frames.title, header.theme.colors.secondaryLabel, frames.titleFrame)
    count.show(frames.count, header.theme.colors.tertiaryLabel, frames.countFrame)
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) info.isHeading = true
  }
}
