package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.view.View
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.roundToInt

/*
 * A card of a snap list, height tall with its margin: a rounded card filled by an image, with a
 * caption of a title and a subtitle over its bottom edge. A snap list gives every card its
 * height. A null height means a quarter of the window's height. height is in dp.
 */
data class ShadowListUtilsSnapCard(
  val item: ShadowListUtilsSnapItem,
  val height: Float?,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsSnapLabels,
) : ShadowListUtilsRow {
  override val key: String get() = item.key
  override val contentVersion: Int = 31 * item.hashCode() + (height?.hashCode() ?: 0)
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsSnapCardView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val spacing = theme.spacing
    val direction = ShadowListUtilsDirection(widthPx)
    val heightPx = height?.px ?: defaultHeightPx(ShadowListUtilsPlatform.windowHeightPx)
    val margin = spacing.sm.px
    val cardFrame = Rect(margin, margin, (widthPx - margin).coerceAtLeast(margin), (heightPx - margin).coerceAtLeast(margin))
    val padding = spacing.lg.px
    val textWidth = (cardFrame.width() - 2 * padding).coerceAtLeast(1)
    val title = item.title?.let { theme.typography.title3.layout(it, textWidth, TEXT_LINES) }
    val subtitle = item.subtitle?.let { theme.typography.subhead.layout(it, textWidth, TEXT_LINES) }

    // Frames inside the card. The caption sits on the card's bottom edge.
    var captionFrame = Rect()
    var titleFrame = Rect()
    var subtitleFrame = Rect()
    if (title != null || subtitle != null) {
      val titleHeight = title?.height ?: 0
      val subtitleHeight = subtitle?.height ?: 0
      val captionHeight = titleHeight + subtitleHeight + 2 * padding
      captionFrame = Rect(0, cardFrame.height() - captionHeight, cardFrame.width(), cardFrame.height())
      titleFrame = title?.let { frameOf(padding, padding, it) } ?: Rect()
      subtitleFrame = subtitle?.let { frameOf(padding, padding + titleHeight, it) } ?: Rect()
    }
    return ShadowListUtilsSnapCardLayout(
      widthPx, heightPx, direction.place(cardFrame), captionFrame, title, direction.place(titleFrame, cardFrame.width()),
      subtitle, direction.place(subtitleFrame, cardFrame.width()))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsSnapCard"

    /*
     * The height of a card without one of its own: a quarter of the window's height, on the pixel
     * grid.
     */
    fun defaultHeightPx(windowHeightPx: Int): Int = (windowHeightPx * WINDOW_SHARE).roundToInt()

    private const val TEXT_LINES = 2
    private const val WINDOW_SHARE = 0.25f
  }
}

/*
 * cardFrame is in the row. captionFrame is in the card, and the text frames are in the caption.
 */
class ShadowListUtilsSnapCardLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val cardFrame: Rect,
  val captionFrame: Rect,
  val title: ShadowListKitTextLayout?,
  val titleFrame: Rect,
  val subtitle: ShadowListKitTextLayout?,
  val subtitleFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsSnapCardView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsSnapCard.REUSE_IDENTIFIER) {
  private val image = ShadowListUtilsImageView(context)
  private val caption = View(context)
  private val title = ShadowListKitTextView(context)
  private val subtitle = ShadowListKitTextView(context)

  private val captionOutline = ShadowListUtilsBottomRoundedOutline(0f)

  init {
    caption.setBackgroundColor(CAPTION_COLOR)
    caption.outlineProvider = captionOutline
    caption.clipToOutline = true
    for (view in listOf(image, caption, title, subtitle)) addView(view)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val card = row as ShadowListUtilsSnapCard
    val item = card.item
    restingBackgroundColor = card.theme.colors.background
    // The image view is the card: its placeholder is the card's color.
    image.cornerRadius = card.theme.radius.lg
    image.placeholderColor = item.color ?: card.theme.colors.elevated
    image.url = item.imageUrl
    captionOutline.radius = card.theme.radius.lg.pxf
    caption.invalidateOutline()
    val text = listOfNotNull(item.title, item.subtitle).filter { it.isNotEmpty() }.joinToString(", ")
    contentDescription = text.ifEmpty { item.imageAlt ?: card.labels.card }
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val frames = layout as ShadowListUtilsSnapCardLayout
    val card = frames.cardFrame
    image.place(card)
    val captionFrame = Rect(frames.captionFrame).apply { offset(card.left, card.top) }
    caption.visibility = if (frames.title == null && frames.subtitle == null) GONE else VISIBLE
    if (caption.visibility == VISIBLE) caption.place(captionFrame)
    title.show(frames.title, TITLE_COLOR, Rect(frames.titleFrame).apply { offset(captionFrame.left, captionFrame.top) })
    subtitle.show(frames.subtitle, SUBTITLE_COLOR, Rect(frames.subtitleFrame).apply { offset(captionFrame.left, captionFrame.top) })
  }

  private companion object {
    const val CAPTION_COLOR = 0x59000000
    const val TITLE_COLOR = 0xFFFFFFFF.toInt()
    const val SUBTITLE_COLOR = 0xCCFFFFFF.toInt()
  }
}
