package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.floor
import kotlin.math.roundToInt

/*
 * A tile of a reorder grid: a colored swatch with the item's label on it and its title under
 * it. aspectRatio, when set, is the swatch's width over its height in place of the item's own,
 * and keeps the title to one line to make every tile the same height. The label keeps its size
 * whatever the text size setting.
 */
data class ShadowListUtilsReorderTile(
  val item: ShadowListUtilsReorderTileItem,
  val aspectRatio: Float?,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsReorderLabels,
) : ShadowListUtilsRow {
  override val key: String get() = item.key
  override val contentVersion: Int = 31 * item.hashCode() + (aspectRatio?.hashCode() ?: 0)
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsReorderTileView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val spacing = theme.spacing
    val direction = ShadowListUtilsDirection(widthPx)
    val padding = PADDING_HORIZONTAL_DP.px
    val textInset = (PADDING_HORIZONTAL_DP + theme.spacing.xs).px
    val swatchWidth = (widthPx - 2 * padding).coerceAtLeast(0)
    val ratio = aspectRatio ?: item.aspectRatio ?: 1f
    /*
     * React Native sizes the swatch from the tile's unrounded content width, floors the title's
     * origin and its padding, and rounds the tile's height once.
     */
    val contentWidth = (widthPx - 2 * PADDING_HORIZONTAL_DP.pxf).coerceAtLeast(0f)
    val swatchHeight = if (ratio > 0f) contentWidth / ratio else contentWidth
    val swatchFrame = Rect(padding, 0, padding + swatchWidth, swatchHeight.roundToInt())
    var label: ShadowListKitTextLayout? = null
    var labelFrame = Rect()
    item.label?.let { text ->
      val typeset = theme.typography.title2.layout(text, swatchWidth.coerceAtLeast(1), 1, scaled = false)
      labelFrame = frameOf(
        swatchFrame.left + (swatchFrame.width() - typeset.width) / 2, swatchFrame.top + (swatchFrame.height() - typeset.height) / 2,
        typeset)
      label = typeset
    }
    val lines = if (aspectRatio != null) 1 else if (ShadowListUtilsPlatform.isLargeText) LARGE_TEXT_TITLE_LINES else TITLE_LINES
    val title = theme.typography.footnote.layout(item.title, (widthPx - 2 * textInset).coerceAtLeast(1), lines)
    val textX = floor(PADDING_HORIZONTAL_DP.pxf).toInt() + floor(spacing.xs.pxf).toInt()
    val titleTop = swatchHeight + spacing.xs.pxf
    val titleFrame = frameOf(textX, floor(titleTop).toInt(), title)
    return ShadowListUtilsReorderTileLayout(
      widthPx, (titleTop + title.height + spacing.md.pxf).roundToInt(), direction.place(swatchFrame), label, direction.place(labelFrame), title,
      direction.place(titleFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsReorderTile"
    private const val PADDING_HORIZONTAL_DP = 6f
    private const val TITLE_LINES = 2
    private const val LARGE_TEXT_TITLE_LINES = 4
  }
}

class ShadowListUtilsReorderTileLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val swatchFrame: Rect,
  val label: ShadowListKitTextLayout?,
  val labelFrame: Rect,
  val title: ShadowListKitTextLayout,
  val titleFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsReorderTileView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsReorderTile.REUSE_IDENTIFIER) {
  private val swatch = View(context)
  private val label = ShadowListKitTextView(context)
  private val title = ShadowListKitTextView(context)
  private val outline = ShadowListUtilsRoundedOutline(0f)

  init {
    swatch.outlineProvider = outline
    swatch.clipToOutline = true
    for (view in listOf(swatch, label, title)) addView(view)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val tile = row as ShadowListUtilsReorderTile
    val item = tile.item
    val colors = tile.theme.colors
    restingBackgroundColor = colors.background
    swatch.setBackgroundColor(
      item.color ?: ShadowListUtilsAvatar.paletteColor(item.label ?: item.title, colors.avatarPalette) ?: colors.fill)
    outline.radius = tile.theme.radius.md.pxf
    swatch.invalidateOutline()
    contentDescription = item.label?.let { "$it, ${item.title}" } ?: item.title
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val tile = row as ShadowListUtilsReorderTile
    val frames = layout as ShadowListUtilsReorderTileLayout
    swatch.place(frames.swatchFrame)
    label.show(frames.label, LABEL_COLOR, frames.labelFrame)
    title.show(frames.title, tile.theme.colors.label, frames.titleFrame)
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    (row as? ShadowListUtilsReorderTile)?.let { info.setHint(it.labels.dragHint) }
  }

  private companion object {
    const val LABEL_COLOR = 0xFFFFFFFF.toInt()
  }
}
