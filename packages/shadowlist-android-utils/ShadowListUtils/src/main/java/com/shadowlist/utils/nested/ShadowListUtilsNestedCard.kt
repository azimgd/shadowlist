package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.floor

/*
 * A card of the horizontal list in a nested row: an image with a title of two lines under it.
 * The row sits in a horizontal list: layout(widthPx) takes the list's height, and the layout's
 * width is the card's size along the list with its leading margin.
 */
data class ShadowListUtilsNestedCard(
  val item: ShadowListUtilsNestedCardItem,
  val theme: ShadowListUtilsTheme,
) : ShadowListUtilsRow {
  override val key: String get() = item.key
  override val contentVersion: Int = item.hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsNestedCardView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val leading = theme.spacing.lg.px
    val imageWidth = IMAGE_WIDTH_DP.px
    val along = leading + imageWidth
    val direction = ShadowListUtilsDirection(along)
    val imageFrame = Rect(leading, 0, along, IMAGE_HEIGHT_DP.px)
    val title = theme.typography.subhead.layout(item.title, imageWidth, TITLE_LINES)
    // React Native floors a text origin to the pixel grid.
    val titleFrame = frameOf(leading, floor((IMAGE_HEIGHT_DP + theme.spacing.sm).pxf).toInt(), title)
    return ShadowListUtilsNestedCardLayout(
      along, titleFrame.bottom, direction.place(imageFrame), title, direction.place(titleFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsNestedCard"

    /*
     * How tall a card is: its image, the gap under it and the lines of its title at the reader's
     * text size, before rounding. A row's list of cards is this tall.
     */
    fun heightPxf(theme: ShadowListUtilsTheme): Float =
      (IMAGE_HEIGHT_DP + theme.spacing.sm).pxf + TITLE_LINES * theme.typography.subhead.lineHeight.spf

    const val IMAGE_WIDTH_DP = 180f
    const val IMAGE_HEIGHT_DP = 220f
    private const val TITLE_LINES = 2
  }
}

class ShadowListUtilsNestedCardLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val imageFrame: Rect,
  val title: ShadowListKitTextLayout,
  val titleFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsNestedCardView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsNestedCard.REUSE_IDENTIFIER) {
  private val image = ShadowListUtilsImageView(context)
  private val title = ShadowListKitTextView(context)

  init {
    addView(image)
    addView(title)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val card = row as ShadowListUtilsNestedCard
    restingBackgroundColor = card.theme.colors.background
    image.cornerRadius = card.theme.radius.md
    image.placeholderColor = card.theme.colors.elevated2
    image.url = card.item.imageUrl
    contentDescription = card.item.imageAlt?.let { "${card.item.title}, $it" } ?: card.item.title
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val card = row as ShadowListUtilsNestedCard
    val frames = layout as ShadowListUtilsNestedCardLayout
    image.place(frames.imageFrame)
    title.show(frames.title, card.theme.colors.label, frames.titleFrame)
  }
}
