package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.drawable.Drawable

/*
 * A tree node at the depth and expansion state the shared tree source returned. icon replaces
 * the folder or file glyph. The host keeps labels and direction in its own locale.
 */
data class ShadowListUtilsTreeRow(
  val item: ShadowListUtilsTreeNode,
  val depth: Int,
  val hasChildren: Boolean,
  val isExpanded: Boolean,
  val indentWidth: Float = 16f,
  val interactive: Boolean = false,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsTreeLabels = ShadowListUtilsTreeLabels(),
  val icon: Drawable? = null,
) : ShadowListUtilsRow {
  override val key: String get() = item.key
  override val contentVersion: Int get() = listOf(item.key, item.name, item.kind, item.children?.size, depth, hasChildren, isExpanded, indentWidth, interactive, icon, labels).hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsTreeRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val direction = ShadowListUtilsDirection(widthPx)
    val spacing = theme.spacing
    val start = (indentWidth.coerceAtLeast(0f) * depth).px
    val count = item.children?.let { theme.typography.footnote.layout(it.size.toString(), (widthPx / 4).coerceAtLeast(1), 1) }
    val countWidth = count?.width ?: 0
    val textX = start + 22f.px + 24f.px + spacing.sm.px
    val textWidth = (widthPx - textX - spacing.lg.px - countWidth - if (count == null) 0 else spacing.sm.px).coerceAtLeast(1)
    val name = theme.typography.callout.layout(item.name, textWidth, if (ShadowListUtilsPlatform.isLargeText) 0 else 1)
    val height = maxOf(44f.px, name.height + spacing.xs.px * 2)
    val glyphSize = 20f.px
    val chevronSize = 14f.px
    val chevronY = (height - chevronSize) / 2
    val glyphY = (height - glyphSize) / 2
    return ShadowListUtilsTreeRowLayout(
      widthPx, height, name, direction.place(frameOf(textX, (height - name.height) / 2, name)),
      direction.place(Rect(start + 4f.px, chevronY, start + 4f.px + chevronSize, chevronY + chevronSize)),
      direction.place(Rect(start + 22f.px, glyphY, start + 22f.px + glyphSize, glyphY + glyphSize)), count,
      direction.place(count?.let { frameOf(widthPx - spacing.lg.px - countWidth, (height - it.height) / 2, it) } ?: Rect()))
  }

  internal companion object { const val REUSE_IDENTIFIER = "ShadowListUtilsTreeRow" }
}
