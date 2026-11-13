package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Button
import android.widget.ImageView
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.roundToInt

/*
 * A card of a masonry grid: an image at its own aspect with an optional title of two lines
 * under it, and of four at a large text size.
 */
data class ShadowListUtilsMasonryCard(
  val item: ShadowListUtilsMasonryItem,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsMasonryLabels,
) : ShadowListUtilsRow {
  override val key: String get() = item.key
  override val contentVersion: Int = item.hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsMasonryCardView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val direction = ShadowListUtilsDirection(widthPx)
    val padding = PADDING_HORIZONTAL_DP.px
    val imageWidth = (widthPx - 2 * padding).coerceAtLeast(0)
    val aspectRatio = if (item.image.height > 0f) item.image.width / item.image.height else 1f
    val imageHeight = if (aspectRatio > 0f) (imageWidth / aspectRatio).roundToInt() else imageWidth
    val imageFrame = Rect(padding, 0, padding + imageWidth, imageHeight)
    var bottom = imageFrame.bottom + theme.spacing.sm.px
    var title: ShadowListKitTextLayout? = null
    var titleFrame = Rect()
    item.title?.let { text ->
      val lines = if (ShadowListUtilsPlatform.isLargeText) LARGE_TEXT_TITLE_LINES else TITLE_LINES
      val typeset = theme.typography.subhead.layout(text, (imageWidth - 2 * theme.spacing.xs.px).coerceAtLeast(1), lines)
      titleFrame = frameOf(padding + theme.spacing.xs.px, bottom, typeset)
      title = typeset
      bottom += typeset.height
    }
    return ShadowListUtilsMasonryCardLayout(
      widthPx, bottom + theme.spacing.md.px, direction.place(imageFrame), title, direction.place(titleFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsMasonryCard"
    const val PADDING_HORIZONTAL_DP = 6f
    private const val TITLE_LINES = 2
    private const val LARGE_TEXT_TITLE_LINES = 4
  }
}

class ShadowListUtilsMasonryCardLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val imageFrame: Rect,
  val title: ShadowListKitTextLayout?,
  val titleFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsMasonryCardView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsMasonryCard.REUSE_IDENTIFIER) {
  private val image = ShadowListUtilsImageView(context)
  private val title = ShadowListKitTextView(context)

  init {
    addView(image)
    addView(title)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val card = row as ShadowListUtilsMasonryCard
    restingBackgroundColor = card.theme.colors.background
    image.cornerRadius = card.theme.radius.md
    image.placeholderColor = card.theme.colors.elevated2
    image.url = card.item.image.url
    contentDescription = card.item.image.alt ?: card.item.title ?: card.labels.image
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val card = row as ShadowListUtilsMasonryCard
    val frames = layout as ShadowListUtilsMasonryCardLayout
    image.place(frames.imageFrame)
    title.show(frames.title, card.theme.colors.label, frames.titleFrame)
  }

  /*
   * A card reads as an image, and as a button in a list that selects on a tap.
   */
  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    if (info.className != Button::class.java.name) info.className = ImageView::class.java.name
  }
}
