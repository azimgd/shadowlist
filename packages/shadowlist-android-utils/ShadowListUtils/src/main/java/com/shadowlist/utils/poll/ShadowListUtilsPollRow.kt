package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import kotlin.math.max

/*
 * An option's label, share and selection. The core computes its percentage with the same
 * rounding as every other template host. An icon replaces the first Unicode scalar of the label.
 */
data class ShadowListUtilsPollRow(
  val item: ShadowListUtilsPollOption,
  val totalVotes: Double,
  val isLeading: Boolean = false,
  val isSelected: Boolean = false,
  val interactive: Boolean = false,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsPollLabels = ShadowListUtilsPollLabels(),
) : ShadowListUtilsRow {
  val percent: Int = ShadowListUtilsCore.pollPercent(item.votes, totalVotes)
  internal val accessibilityLabel = labels.option(item.label, percent)
  override val key: String get() = item.key
  override val contentVersion: Int = listOf(item, totalVotes, isLeading, isSelected, interactive, accessibilityLabel).hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsPollRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val spacing = theme.spacing
    val typography = theme.typography
    val direction = ShadowListUtilsDirection(widthPx)
    val top = 14f.px
    val selectionSize = SELECTION_SIZE_DP.px
    val selectionFrame = Rect(spacing.lg.px, top, spacing.lg.px + selectionSize, top + selectionSize)
    val initialText = if (item.label.isEmpty()) "" else String(Character.toChars(item.label.codePointAt(0))).uppercase(java.util.Locale.ROOT)
    val initial = typography.headline.layout(initialText, selectionSize, 1, scaled = true, centered = true)
    val initialFrame = Rect(selectionFrame.left, top + (selectionSize - initial.height) / 2,
      selectionFrame.right, top + (selectionSize + initial.height) / 2)
    val bodyX = selectionFrame.right + 14f.px
    val bodyWidth = (widthPx - bodyX - spacing.lg.px).coerceAtLeast(1)
    val percent = typography.subhead.with(if (isLeading) ShadowListUtilsFontWeight.SEMIBOLD else ShadowListUtilsFontWeight.REGULAR).layout("${this.percent}%", (bodyWidth / 3).coerceAtLeast(1), 1)
    val percentFrame = frameOf(widthPx - spacing.lg.px - percent.width, top, percent)
    val label = typography.headline.layout(item.label, (bodyWidth - percent.width - spacing.sm.px).coerceAtLeast(1))
    val labelFrame = frameOf(bodyX, top, label)
    val barTop = max(labelFrame.bottom, percentFrame.bottom) + spacing.sm.px
    val barFrame = Rect(bodyX, barTop, bodyX + bodyWidth, barTop + BAR_HEIGHT_DP.px)
    val fillWidth = (bodyWidth * this.percent.coerceIn(0, 100) / 100.0).toInt()
    val barFillFrame = Rect(bodyX, barTop, bodyX + fillWidth, barFrame.bottom)
    return ShadowListUtilsPollRowLayout(
      widthPx, max(selectionFrame.bottom, barFrame.bottom) + spacing.md.px,
      direction.place(selectionFrame), initial, direction.place(initialFrame), label, direction.place(labelFrame), percent, direction.place(percentFrame),
      direction.place(barFrame), direction.place(barFillFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsPollRow"
    const val SELECTION_SIZE_DP = 44f
    private const val BAR_HEIGHT_DP = 6f
  }
}
