package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/*
 * A chat message. Own messages sit at the trailing edge in the accent color. A message from
 * someone else shows the sender's name and avatar unless showsAuthor is false, as in the rest
 * of a run from one author: the avatar's place stays empty and the bubbles line up. caption is
 * a small line inside the bubble, or under the images of a message without text. An empty
 * caption is no caption.
 *
 * The layout is the row's size prediction. The bubble's share of the row and its padding come
 * from the core.
 */
data class ShadowListUtilsChatBubble(
  val item: ShadowListUtilsChatMessage,
  val caption: String?,
  val showsAuthor: Boolean,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsChatLabels,
) : ShadowListUtilsRow {
  override val key: String get() = item.key
  override val contentVersion: Int = 31 * (31 * item.hashCode() + (caption?.hashCode() ?: 0)) + showsAuthor.hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsChatBubbleView(context)

  private val text: String? get() = item.text?.takeIf { it.isNotEmpty() }

  /*
   * The caption the row shows: null for an empty one.
   */
  internal val shownCaption: String? get() = caption?.takeIf { it.isNotEmpty() }

  internal val failed: Boolean get() = item.status == ShadowListUtilsChatMessageStatus.FAILED
  internal val sending: Boolean get() = item.status == ShadowListUtilsChatMessageStatus.SENDING

  /*
   * The images shown: one alone, or the first of several in a grid.
   */
  internal val shownImages: List<String> get() = item.images.take(GRID_LIMIT)

  /*
   * What the core sizes a text message from, into out. The metrics are the theme's, with the
   * line heights text is laid out with, which follow the text size setting. A message with
   * images still asks as a text message, for the width of the bubble under them.
   */
  private fun sizeSpec(out: DoubleArray) {
    var shape = ShadowListUtilsCore.CHAT_SHAPE_HAS_TEXT
    if (item.isOwn) shape = shape or ShadowListUtilsCore.CHAT_SHAPE_IS_OWN
    if (showsAuthor) shape = shape or ShadowListUtilsCore.CHAT_SHAPE_SHOWS_AUTHOR
    if (shownCaption != null) shape = shape or ShadowListUtilsCore.CHAT_SHAPE_HAS_CAPTION
    if (failed) shape = shape or ShadowListUtilsCore.CHAT_SHAPE_FAILED
    val metrics = DoubleArray(ShadowListUtilsCore.CHAT_METRIC_SLOTS)
    val body = theme.typography.body
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_XXS] = theme.spacing.xxs.toDouble()
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_XS] = theme.spacing.xs.toDouble()
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_SM] = theme.spacing.sm.toDouble()
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_MD] = theme.spacing.md.toDouble()
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_LG] = theme.spacing.lg.toDouble()
    metrics[ShadowListUtilsCore.CHAT_METRIC_BODY_FONT_SIZE] = body.fontSize.toDouble()
    metrics[ShadowListUtilsCore.CHAT_METRIC_BODY_LINE_HEIGHT] = body.scaledLineHeightPx().toFloat().dp.toDouble()
    metrics[ShadowListUtilsCore.CHAT_METRIC_BODY_LETTER_SPACING] = body.letterSpacing.toDouble()
    metrics[ShadowListUtilsCore.CHAT_METRIC_CAPTION_LINE_HEIGHT] = theme.typography.caption.scaledLineHeightPx().toFloat().dp.toDouble()
    ShadowListUtilsCore.chatMessageSizeSpec(shape, metrics, out)
  }

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val spacing = theme.spacing
    val typography = theme.typography
    val direction = ShadowListUtilsDirection(widthPx)
    val own = item.isOwn
    val images = shownImages
    val text = text
    val caption = shownCaption
    val captionLine = typography.caption.scaledLineHeightPx()
    val avatarSize = AVATAR_SIZE_DP.px
    // Others keep the avatar's column whether or not the avatar shows.
    val leading = if (own) spacing.md.px else (spacing.md + AVATAR_SIZE_DP + spacing.sm).px
    val trailing = widthPx - spacing.md.px

    val spec = DoubleArray(ShadowListUtilsCore.CHAT_OUT_SLOTS)
    sizeSpec(spec)
    val fraction = spec[ShadowListUtilsCore.CHAT_OUT_WIDTH_FRACTION]
    val textWidth = max(1, floor(fraction * widthPx - spec[ShadowListUtilsCore.CHAT_OUT_INSET_WIDTH].toFloat().pxf).toInt())
    val bubblePadding = ShadowListUtilsCore.CHAT_BUBBLE_PADDING_HORIZONTAL_DP.px
    val columnLimit = textWidth + 2 * bubblePadding

    var name: ShadowListKitTextLayout? = null
    var body: ShadowListKitTextLayout? = null
    var captionText: ShadowListKitTextLayout? = null
    var failedText: ShadowListKitTextLayout? = null
    var nameFrame = Rect()
    var bubbleFrame = Rect()
    var textFrame = Rect()
    var captionFrame = Rect()
    var failedFrame = Rect()
    var imagesFrame = Rect()

    var y = (2 * spacing.xxs).px - spacing.xxs.px
    var columnWidth = 0
    if (images.isNotEmpty()) {
      val imagesWidth = IMAGES_WIDTH_DP.px
      val left = if (own) trailing - imagesWidth else leading
      imagesFrame = Rect(left, y + spacing.xxs.px, left + imagesWidth, y + spacing.xxs.px + imagesHeight(images.size, spacing.xxs.px))
      y = imagesFrame.bottom + spacing.xxs.px
      columnWidth = imagesWidth
    } else if (!own && showsAuthor && text != null) {
      val line = typography.caption.layout(item.author.name, max(1, columnLimit - spacing.md.px), 1)
      name = line
      nameFrame = frameOf(leading + spacing.md.px, y, line)
      y += captionLine + spacing.xxs.px
      columnWidth = line.width + spacing.md.px
    }

    if (text != null) {
      val typeset = typography.body.layout(text, textWidth)
      var innerWidth = if (typeset.lineCount > 1) textWidth else typeset.width
      var innerHeight = typeset.height
      if (caption != null) {
        val line = typography.caption.layout(caption, textWidth, 1)
        captionText = line
        innerWidth = max(innerWidth, line.width)
        innerHeight += spacing.xs.px + captionLine
      }
      val bubbleWidth = innerWidth + 2 * bubblePadding
      val bubbleX = if (own) trailing - bubbleWidth else leading
      bubbleFrame = Rect(bubbleX, y, bubbleX + bubbleWidth, y + innerHeight + 2 * spacing.sm.px)
      body = typeset
      textFrame = frameOf(bubbleX + bubblePadding, y + spacing.sm.px, typeset)
      captionText?.let { captionFrame = frameOf(bubbleX + bubblePadding, textFrame.bottom + spacing.xs.px, it) }
      y = bubbleFrame.bottom
      columnWidth = max(columnWidth, bubbleWidth)
    } else if (caption != null) {
      // A caption without text sits under the images, on the side the message is on.
      val line = typography.caption.layout(caption, textWidth, 1)
      captionText = line
      captionFrame = frameOf(if (own) trailing - line.width else leading, y + spacing.xs.px, line)
      columnWidth = max(columnWidth, line.width)
      y = captionFrame.top + captionLine
    }

    if (failed) {
      // The failed line ends where the column of the message ends.
      val line = typography.caption.layout(labels.failed, columnLimit, 1)
      columnWidth = max(columnWidth, line.width)
      val columnTrailing = if (own) trailing else min(trailing, leading + columnWidth)
      failedText = line
      failedFrame = frameOf(columnTrailing - line.width, y + spacing.xs.px, line)
      y = failedFrame.top + captionLine
    }
    if (images.size > 1) y += spacing.xxs.px

    // A text message is as tall as the core predicts. A message with images is measured here.
    var height = if (images.isEmpty() && body != null) {
      body.height + spec[ShadowListUtilsCore.CHAT_OUT_INSET_HEIGHT].toFloat().px
    } else {
      y + spacing.xxs.px
    }
    var avatarFrame = Rect()
    if (!own) {
      // The avatar sits at the bottom of the row, over the row's padding and its own margin.
      height = max(height, avatarSize + 3 * spacing.xxs.px)
      val bottom = height - 2 * spacing.xxs.px
      avatarFrame = Rect(spacing.md.px, bottom - avatarSize, spacing.md.px + avatarSize, bottom)
    }
    val imageFrames = imageFrames(images.size, spacing.xxs.px).map { direction.place(it, imagesFrame.width()) }
    return ShadowListUtilsChatBubbleLayout(
      widthPx, height, columnLimit, direction.place(avatarFrame), name, direction.place(nameFrame), direction.place(bubbleFrame),
      body, direction.place(textFrame), captionText, direction.place(captionFrame), failedText, direction.place(failedFrame),
      direction.place(imagesFrame), imageFrames)
  }

  /*
   * One image shows large. Several show as a grid of square tiles in two columns.
   */
  private fun imagesHeight(count: Int, gap: Int): Int {
    if (count <= 1) return SINGLE_IMAGE_HEIGHT_DP.px
    val lines = (count + 1) / 2
    return lines * GRID_TILE_SIZE_DP.px + (lines - 1) * gap
  }

  private fun imageFrames(count: Int, gap: Int): List<Rect> {
    if (count <= 1) return if (count == 1) listOf(Rect(0, 0, IMAGES_WIDTH_DP.px, SINGLE_IMAGE_HEIGHT_DP.px)) else emptyList()
    val tile = GRID_TILE_SIZE_DP.px
    return List(count) { index ->
      val left = (index % 2) * (tile + gap)
      val top = (index / 2) * (tile + gap)
      Rect(left, top, left + tile, top + tile)
    }
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsChatBubble"
    const val AVATAR_SIZE_DP = 30f
    const val GRID_LIMIT = 4
    private const val IMAGES_WIDTH_DP = 240f
    private const val SINGLE_IMAGE_HEIGHT_DP = 320f
    private const val GRID_TILE_SIZE_DP = 119f
  }
}

/*
 * imageFrames are in imagesFrame. maximumBubbleWidthPx is the widest a bubble gets at this width.
 */
class ShadowListUtilsChatBubbleLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val maximumBubbleWidthPx: Int,
  val avatarFrame: Rect,
  val name: ShadowListKitTextLayout?,
  val nameFrame: Rect,
  val bubbleFrame: Rect,
  val text: ShadowListKitTextLayout?,
  val textFrame: Rect,
  val caption: ShadowListKitTextLayout?,
  val captionFrame: Rect,
  val failed: ShadowListKitTextLayout?,
  val failedFrame: Rect,
  val imagesFrame: Rect,
  val imageFrames: List<Rect>,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsChatBubbleView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsChatBubble.REUSE_IDENTIFIER) {
  private val avatar = ShadowListUtilsAvatar(context, ShadowListUtilsChatBubble.AVATAR_SIZE_DP, ShadowListUtilsTheme.LIGHT)
  private val name = ShadowListKitTextView(context)
  private val bubble = View(context)
  private val text = ShadowListKitTextView(context)
  private val caption = ShadowListKitTextView(context)
  private val failed = ShadowListKitTextView(context)
  private val retry = View(context)
  private val imageBlock = FrameLayout(context)
  private val images = List(ShadowListUtilsChatBubble.GRID_LIMIT) { ShadowListUtilsImageView(context) }
  private val bubbleShape = GradientDrawable()

  init {
    bubble.background = bubbleShape
    images.forEach { imageBlock.addView(it) }
    for (view in listOf(avatar, name, bubble, text, caption, failed, retry, imageBlock)) addView(view)
    // A screen reader retries through the row's action.
    retry.importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
    retry.setOnClickListener { retryMessage() }
    retry.isClickable = false
  }

  override fun fill(row: ShadowListUtilsRow) {
    val message = row as ShadowListUtilsChatBubble
    val item = message.item
    val theme = message.theme
    val colors = theme.colors
    val own = item.isOwn
    restingBackgroundColor = colors.background
    avatar.visibility = if (own || !message.showsAuthor) GONE else VISIBLE
    if (avatar.visibility == VISIBLE) {
      avatar.theme = theme
      avatar.show(item.author.name, item.author.avatarUrl, item.author.avatarColor)
    }

    // The small corner is on the side the message is on, which a right to left layout swaps.
    val radius = (theme.radius.lg + RADIUS_EXTRA_DP).pxf
    val tail = TAIL_RADIUS_DP.pxf
    val tailAtRight = own != ShadowListUtilsPlatform.isRightToLeft
    val bottomRight = if (tailAtRight) tail else radius
    val bottomLeft = if (tailAtRight) radius else tail
    bubbleShape.cornerRadii = floatArrayOf(radius, radius, radius, radius, bottomRight, bottomRight, bottomLeft, bottomLeft)
    bubbleShape.setColor(if (own) colors.blue else colors.elevated2)
    val opacity = if (message.sending) SENDING_OPACITY else 1f
    for (view in listOf(bubble, text, caption)) view.alpha = opacity

    val shown = message.shownImages
    imageBlock.visibility = if (shown.isEmpty()) GONE else VISIBLE
    for ((index, view) in images.withIndex()) {
      view.visibility = if (index < shown.size) VISIBLE else GONE
      view.cornerRadius = if (shown.size > 1) theme.radius.sm else theme.radius.lg
      view.placeholderColor = colors.elevated2
      view.url = shown.getOrNull(index)
    }
    retry.isClickable = retries
    val parts = listOf(item.author.name.takeIf { !own }, item.text) + shown.map { message.labels.image } +
      listOf(message.shownCaption, message.labels.failed.takeIf { message.failed })
    contentDescription = parts.filterNotNull().filter { it.isNotEmpty() }.joinToString(", ")
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val message = row as ShadowListUtilsChatBubble
    val frames = layout as ShadowListUtilsChatBubbleLayout
    val colors = message.theme.colors
    val own = message.item.isOwn
    if (avatar.visibility == VISIBLE) avatar.place(frames.avatarFrame)
    name.show(frames.name, colors.secondaryLabel, frames.nameFrame)
    bubble.visibility = if (frames.text == null) GONE else VISIBLE
    if (frames.text != null) bubble.place(frames.bubbleFrame)
    text.show(frames.text, if (own) colors.onAccent else colors.label, frames.textFrame)
    caption.show(
      frames.caption,
      if (frames.text != null && own) colors.onAccent.withOpacity(OWN_CAPTION_OPACITY) else colors.secondaryLabel,
      frames.captionFrame)
    failed.show(frames.failed, colors.red, frames.failedFrame)
    retry.visibility = if (frames.failed == null) GONE else VISIBLE
    if (frames.failed != null) retry.place(Rect(frames.failedFrame).apply { inset(-RETRY_SLOP_DP.px, -RETRY_SLOP_DP.px) })
    if (imageBlock.visibility == VISIBLE) {
      imageBlock.place(frames.imagesFrame)
      for ((view, frame) in images.zip(frames.imageFrames)) view.place(frame)
    }
  }

  private val chatList: ShadowListUtilsChatList? get() = enclosingList as? ShadowListUtilsChatList

  /*
   * Whether a point in the row's coordinates is on the bubble, the part of a message a hold is
   * for.
   */
  internal fun bubbleContains(x: Float, y: Float): Boolean =
    bubble.visibility == VISIBLE && x >= bubble.left && x < bubble.right && y >= bubble.top && y < bubble.bottom

  private fun retryMessage() {
    val message = row as? ShadowListUtilsChatBubble ?: return
    chatList?.onRetryMessage?.invoke(message.item)
  }

  /*
   * The failed line takes a tap only in a list that listens for it. Otherwise the tap is the
   * row's.
   */
  private val retries: Boolean
    get() = (row as? ShadowListUtilsChatBubble)?.failed == true && chatList?.onRetryMessage != null

  /*
   * A cell can be filled before it is in its list. The failed line learns here whether the
   * list listens for its tap.
   */
  override fun onAttachedToWindow() {
    super.onAttachedToWindow()
    retry.isClickable = retries
  }

  override fun claimsTouch(x: Float, y: Float): Boolean =
    retries && retry.visibility == VISIBLE && x >= retry.left && x < retry.right && y >= retry.top && y < retry.bottom

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    val message = row as? ShadowListUtilsChatBubble ?: return
    if (message.sending) info.setHint(message.labels.sending)
  }

  private companion object {
    const val RADIUS_EXTRA_DP = 2f
    const val TAIL_RADIUS_DP = 5f
    const val SENDING_OPACITY = 0.6f
    const val OWN_CAPTION_OPACITY = 0.65f
    const val RETRY_SLOP_DP = 8f
  }
}
