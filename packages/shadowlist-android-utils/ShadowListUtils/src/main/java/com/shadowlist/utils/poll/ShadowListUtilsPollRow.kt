package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import kotlin.math.max
import kotlin.math.roundToInt

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
    val selectionSize = SELECTION_SIZE_DP.px
    val bodyX = spacing.lg.px + selectionSize + SELECTION_GAP_DP.px
    val bodyWidth = (widthPx - bodyX - spacing.lg.px).coerceAtLeast(1)
    val percent = typography.subhead.with(if (isLeading) ShadowListUtilsFontWeight.SEMIBOLD else ShadowListUtilsFontWeight.REGULAR).layout("${this.percent}%", (bodyWidth / 3).coerceAtLeast(1), 1)
    val label = typography.headline.layout(item.label, (bodyWidth - percent.width - spacing.sm.px).coerceAtLeast(1))
    // The selection and the body are centered against the taller of the two.
    val bodyTopHeight = max(label.height, percent.height)
    val bodyHeight = bodyTopHeight + spacing.sm.px + BAR_HEIGHT_DP.px
    // The vertical padding rounds once with the selection, the way React Native rounds the row.
    val heightPx = max((2 * PADDING_VERTICAL_DP + SELECTION_SIZE_DP).px, bodyHeight + (2 * PADDING_VERTICAL_DP).px)
    val selectionTop = ((heightPx - selectionSize) / 2f).roundToInt()
    val bodyTop = (heightPx - bodyHeight) / 2
    val selectionFrame = Rect(spacing.lg.px, selectionTop, spacing.lg.px + selectionSize, selectionTop + selectionSize)
    val initialText = if (item.label.isEmpty()) "" else String(Character.toChars(item.label.codePointAt(0))).uppercase(java.util.Locale.ROOT)
    val initial = typography.headline.layout(initialText, selectionSize, 1, scaled = true, centered = true)
    val initialFrame = Rect(selectionFrame.left, selectionTop + (selectionSize - initial.height) / 2,
      selectionFrame.right, selectionTop + (selectionSize + initial.height) / 2)
    val percentFrame = frameOf(widthPx - spacing.lg.px - percent.width, bodyTop + (bodyTopHeight - percent.height) / 2, percent)
    val labelFrame = frameOf(bodyX, bodyTop + (bodyTopHeight - label.height) / 2, label)
    val barTop = bodyTop + bodyTopHeight + spacing.sm.px
    val barFrame = Rect(bodyX, barTop, bodyX + bodyWidth, barTop + BAR_HEIGHT_DP.px)
    val fillWidth = (bodyWidth * this.percent.coerceIn(0, 100) / 100.0).toInt()
    val barFillFrame = Rect(bodyX, barTop, bodyX + fillWidth, barFrame.bottom)
    return ShadowListUtilsPollRowLayout(
      widthPx, heightPx,
      direction.place(selectionFrame), initial, direction.place(initialFrame), label, direction.place(labelFrame), percent, direction.place(percentFrame),
      direction.place(barFrame), direction.place(barFillFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsPollRow"
    const val SELECTION_SIZE_DP = 44f
    private const val SELECTION_GAP_DP = 14f
    private const val PADDING_VERTICAL_DP = 14f
    private const val BAR_HEIGHT_DP = 6f
  }
}
