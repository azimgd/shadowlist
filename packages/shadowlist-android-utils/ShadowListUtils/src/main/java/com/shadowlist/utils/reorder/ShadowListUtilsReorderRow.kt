package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView

/*
 * A contact with a grip at the trailing edge, for a list whose items are dragged into a new
 * order.
 */
data class ShadowListUtilsReorderRow(
  val item: ShadowListUtilsContactItem,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsReorderLabels,
) : ShadowListUtilsRow {
  override val key: String get() = item.key
  override val contentVersion: Int = item.hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsReorderRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val direction = ShadowListUtilsDirection(widthPx)
    val trailing = theme.spacing.lg.px
    val grip = ShadowListUtilsContactBody.ICON_SIZE_DP.px
    val body = ShadowListUtilsContactBody(item, theme, widthPx, trailing + grip)
    val gripTop = (body.heightPx - grip) / 2
    val gripFrame = Rect(widthPx - trailing - grip, gripTop, widthPx - trailing, gripTop + grip)
    return ShadowListUtilsReorderRowLayout(
      widthPx, body.heightPx, direction.place(body.avatarFrame), body.name, direction.place(body.nameFrame), body.subtitle,
      direction.place(body.subtitleFrame), direction.place(gripFrame), direction.place(body.separatorFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsReorderRow"
  }
}

class ShadowListUtilsReorderRowLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val avatarFrame: Rect,
  val name: ShadowListKitTextLayout,
  val nameFrame: Rect,
  val subtitle: ShadowListKitTextLayout?,
  val subtitleFrame: Rect,
  val gripFrame: Rect,
  val separatorFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsReorderRowView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsReorderRow.REUSE_IDENTIFIER) {
  private val avatar = ShadowListUtilsAvatar(context, ShadowListUtilsContactBody.AVATAR_SIZE_DP, ShadowListUtilsTheme.LIGHT)
  private val name = ShadowListKitTextView(context)
  private val subtitle = ShadowListKitTextView(context)
  private val grip = ShadowListUtilsIconView(context, ShadowListUtilsIconView.Kind.GRIP)
  private val separator = View(context)

  init {
    for (view in listOf(avatar, name, subtitle, grip, separator)) addView(view)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val reorder = row as ShadowListUtilsReorderRow
    val colors = reorder.theme.colors
    restingBackgroundColor = colors.background
    separator.setBackgroundColor(colors.separator)
    grip.color = colors.tertiaryLabel
    avatar.theme = reorder.theme
    avatar.show(reorder.item.name, reorder.item.avatarUrl, reorder.item.avatarColor)
    contentDescription = reorder.item.accessibilityLabel
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val reorder = row as ShadowListUtilsReorderRow
    val frames = layout as ShadowListUtilsReorderRowLayout
    avatar.place(frames.avatarFrame)
    name.show(frames.name, reorder.theme.colors.label, frames.nameFrame)
    subtitle.show(frames.subtitle, reorder.theme.colors.secondaryLabel, frames.subtitleFrame)
    grip.place(frames.gripFrame)
    separator.place(frames.separatorFrame)
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    (row as? ShadowListUtilsReorderRow)?.let { info.setHint(it.labels.dragHint) }
  }
}
