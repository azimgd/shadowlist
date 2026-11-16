package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import kotlin.math.floor
import kotlin.math.roundToInt

/*
 * A board card's title, optional label and detail, with an optional color bar. Its cell stays
 * transparent and the column paints behind the rounded card, including while the card lifts.
 */
data class ShadowListUtilsBoardCard(
  override val key: String,
  val title: String,
  val label: String? = null,
  val detail: String? = null,
  val color: Int? = null,
  val accessibilityLabel: String? = null,
  val theme: ShadowListUtilsTheme,
) : ShadowListUtilsRow {
  override val contentVersion: Int get() = listOf(title, label, detail, color, accessibilityLabel).hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsBoardCardView(context)
  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val inset = theme.spacing.md.px
    val border = ShadowListUtilsTheme.HAIRLINE_PX
    val accentWidth = if (color == null) 0f else ACCENT_WIDTH_DP
    /*
     * React Native rounds each view's origin inside its parent, takes a view's size from its
     * rounded edges and floors a text origin. The accent, the body and the row under the title
     * follow that order.
     */
    val accent = (theme.spacing.md + accentWidth).px - inset
    val bodyX = inset + (border + accentWidth.pxf).roundToInt()
    val textX = bodyX + floor(theme.spacing.md.pxf).toInt()
    val metaX = bodyX + theme.spacing.md.px
    val textRight = widthPx - (2 * theme.spacing.md).px - border
    val textWidth = (textRight - textX).coerceAtLeast(1)
    val metaRight = metaX + textWidth
    val title = theme.typography.subhead.layout(this.title, textWidth)
    val due = detail?.let { theme.typography.caption.layout(it, textWidth / 2, 1) }
    val dotSize = if (color == null) 0 else DOT_SIZE_DP.px
    val dotGap = if (color == null) 0 else floor((DOT_SIZE_DP + theme.spacing.xs).pxf).toInt()
    val detail = label?.let {
      theme.typography.caption.with(ShadowListUtilsFontWeight.SEMIBOLD).layout(it,
        (textWidth - (due?.width ?: 0) - dotGap - theme.spacing.xs.px).coerceAtLeast(1), 1)
    }
    val top = border + floor(theme.spacing.md.pxf).toInt()
    val metaTop = border + (theme.spacing.md.pxf + title.height + theme.spacing.xs.pxf).roundToInt()
    val metaHeight = maxOf(detail?.height ?: 0, due?.height ?: 0)
    val dotTop = metaTop + ((metaHeight - dotSize) / 2f).roundToInt()
    val cardHeight = (2 * border + (2 * theme.spacing.md).pxf +
      title.lineCount * theme.typography.subhead.lineHeight.spf +
      if (detail != null || due != null) theme.spacing.xs.pxf + metaHeight else 0f).roundToInt()
    return ShadowListUtilsBoardCardLayout(widthPx, cardHeight + theme.spacing.sm.px,
      Rect(inset, 0, widthPx - inset, cardHeight), Rect(inset + border, border, inset + border + accent, cardHeight - border),
      title, frameOf(textX, top, title), detail,
      detail?.let { frameOf(metaX + dotGap, metaTop, it) } ?: Rect(),
      due, due?.let { frameOf(metaRight - it.width, metaTop, it) } ?: Rect(),
      if (detail != null && dotSize > 0) Rect(metaX, dotTop, metaX + dotSize, dotTop + dotSize) else Rect())
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsBoardCard"
    private const val ACCENT_WIDTH_DP = 4f
    private const val DOT_SIZE_DP = 8f
  }
}
