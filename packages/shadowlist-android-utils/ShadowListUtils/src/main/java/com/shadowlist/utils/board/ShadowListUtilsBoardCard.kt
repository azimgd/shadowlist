package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect

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
    val textWidth = (widthPx - inset * 4).coerceAtLeast(1)
    val title = theme.typography.headline.layout(this.title, textWidth)
    val meta = listOfNotNull(label, detail).joinToString(" · ")
    val detail = meta.takeIf { it.isNotEmpty() }?.let { theme.typography.footnote.layout(it, textWidth) }
    val accent = if (color == null) 0 else theme.spacing.xs.px
    val height = inset * 3 + accent + title.height + (detail?.let { theme.spacing.sm.px + it.height } ?: 0)
    return ShadowListUtilsBoardCardLayout(widthPx, height,
      Rect(inset, 0, widthPx - inset, height - inset), Rect(inset * 2, inset, inset * 2 + textWidth, inset + accent),
      title, frameOf(inset * 2, inset + accent, title), detail,
      detail?.let { frameOf(inset * 2, inset + accent + title.height + theme.spacing.sm.px, it) } ?: Rect())
  }
  internal companion object { const val REUSE_IDENTIFIER = "ShadowListUtilsBoardCard" }
}
