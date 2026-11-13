package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.view.View
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.max

/*
 * A contact: an avatar, a name and an optional subtitle of one line each, with a chevron at the
 * trailing edge when showsChevron is set. At a large text size both lines wrap. The row
 * highlights while pressed in a list that selects.
 */
data class ShadowListUtilsContactRow(
  val item: ShadowListUtilsContactItem,
  val showsChevron: Boolean,
  val theme: ShadowListUtilsTheme,
) : ShadowListUtilsRow {
  override val key: String get() = item.key
  override val contentVersion: Int = 31 * item.hashCode() + showsChevron.hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsContactRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val direction = ShadowListUtilsDirection(widthPx)
    val trailing = theme.spacing.md.px
    val accessory = if (showsChevron) ShadowListUtilsContactBody.ICON_SIZE_DP.px else 0
    val body = ShadowListUtilsContactBody(item, theme, widthPx, trailing + accessory)
    val chevronFrame = if (showsChevron) {
      val top = (body.heightPx - accessory) / 2
      Rect(widthPx - trailing - accessory, top, widthPx - trailing, top + accessory)
    } else {
      Rect()
    }
    return ShadowListUtilsContactRowLayout(
      widthPx, body.heightPx, direction.place(body.avatarFrame), body.name, direction.place(body.nameFrame), body.subtitle,
      direction.place(body.subtitleFrame), direction.place(chevronFrame), direction.place(body.separatorFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsContactRow"
  }
}

/*
 * The frames a contact row and a reorder row share, for a layout that runs from left to right:
 * the avatar, the two lines of text beside it and the hairline under them. trailingPx is the
 * space the row keeps free after the text.
 */
internal class ShadowListUtilsContactBody(
  item: ShadowListUtilsContactItem,
  theme: ShadowListUtilsTheme,
  widthPx: Int,
  trailingPx: Int,
) {
  val avatarFrame: Rect
  val name: ShadowListKitTextLayout
  val nameFrame: Rect
  val subtitle: ShadowListKitTextLayout?
  val subtitleFrame: Rect
  val separatorFrame: Rect
  val heightPx: Int

  init {
    val spacing = theme.spacing
    val avatarSize = AVATAR_SIZE_DP.px
    val textX = spacing.lg.px + avatarSize + spacing.md.px
    val textWidth = (widthPx - textX - trailingPx).coerceAtLeast(1)
    val lines = if (ShadowListUtilsPlatform.isLargeText) 0 else 1
    name = theme.typography.body.layout(item.name, textWidth, lines)
    subtitle = item.subtitle?.takeIf { it.isNotEmpty() }?.let { theme.typography.subhead.layout(it, textWidth, lines) }
    val subtitleGap = SUBTITLE_GAP_DP.px
    val textHeight = name.height + (subtitle?.let { subtitleGap + it.height } ?: 0)
    heightPx = max(avatarSize, textHeight) + 2 * spacing.md.px
    val textY = (heightPx - textHeight) / 2
    val avatarY = (heightPx - avatarSize) / 2
    avatarFrame = Rect(spacing.lg.px, avatarY, spacing.lg.px + avatarSize, avatarY + avatarSize)
    nameFrame = frameOf(textX, textY, name)
    subtitleFrame = subtitle?.let { frameOf(textX, textY + name.height + subtitleGap, it) } ?: Rect()
    separatorFrame = Rect(theme.rowInset.px, heightPx - ShadowListUtilsTheme.HAIRLINE_PX, widthPx, heightPx)
  }

  companion object {
    const val AVATAR_SIZE_DP = 40f
    const val ICON_SIZE_DP = 20f
    private const val SUBTITLE_GAP_DP = 1f
  }
}

/*
 * What a screen reader says for the contact.
 */
internal val ShadowListUtilsContactItem.accessibilityLabel: String
  get() = if (subtitle.isNullOrEmpty()) name else "$name, $subtitle"

class ShadowListUtilsContactRowLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val avatarFrame: Rect,
  val name: ShadowListKitTextLayout,
  val nameFrame: Rect,
  val subtitle: ShadowListKitTextLayout?,
  val subtitleFrame: Rect,
  val chevronFrame: Rect,
  val separatorFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsContactRowView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsContactRow.REUSE_IDENTIFIER) {
  private val avatar = ShadowListUtilsAvatar(context, ShadowListUtilsContactBody.AVATAR_SIZE_DP, ShadowListUtilsTheme.LIGHT)
  private val name = ShadowListKitTextView(context)
  private val subtitle = ShadowListKitTextView(context)
  private val chevron = ShadowListUtilsIconView(context, ShadowListUtilsIconView.Kind.CHEVRON)
  private val separator = View(context)

  init {
    for (view in listOf(avatar, name, subtitle, chevron, separator)) addView(view)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val contact = row as ShadowListUtilsContactRow
    val colors = contact.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = colors.fill
    separator.setBackgroundColor(colors.separator)
    chevron.color = colors.tertiaryLabel
    chevron.visibility = if (contact.showsChevron) VISIBLE else GONE
    avatar.theme = contact.theme
    avatar.show(contact.item.name, contact.item.avatarUrl, contact.item.avatarColor)
    contentDescription = contact.item.accessibilityLabel
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val contact = row as ShadowListUtilsContactRow
    val frames = layout as ShadowListUtilsContactRowLayout
    avatar.place(frames.avatarFrame)
    name.show(frames.name, contact.theme.colors.label, frames.nameFrame)
    subtitle.show(frames.subtitle, contact.theme.colors.secondaryLabel, frames.subtitleFrame)
    if (contact.showsChevron) {
      chevron.place(frames.chevronFrame)
      chevron.invalidate()
    }
    separator.place(frames.separatorFrame)
  }
}
