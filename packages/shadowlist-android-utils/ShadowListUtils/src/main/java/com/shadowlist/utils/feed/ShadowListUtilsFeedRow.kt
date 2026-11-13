package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Button
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.ImageView
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.max
import kotlin.math.roundToInt

/*
 * A post: an avatar, a line with the author's name, handle and the post's age, the text, and
 * one image or a strip of several. formatTime writes the age in place of the labels' words.
 * The age is worked out once, when the row is made.
 */
class ShadowListUtilsFeedRow(
  val item: ShadowListUtilsFeedItem,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsFeedLabels,
  formatTime: ((Double) -> String)? = null,
) : ShadowListUtilsRow {
  /*
   * The post's age in words, or null for a post without a time and for a placeholder.
   */
  val time: String? = item.createdAtMs?.takeIf { !item.placeholder }?.let {
    formatTime?.invoke(it) ?: labels.time.format(it, System.currentTimeMillis().toDouble())
  }

  override val key: String get() = item.key
  override val contentVersion: Int = 31 * item.hashCode() + (time?.hashCode() ?: 0)
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsFeedRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val spacing = theme.spacing
    val subhead = theme.typography.subhead
    val direction = ShadowListUtilsDirection(widthPx)
    val avatarSize = AVATAR_SIZE_DP.px
    val gap = spacing.xs.px
    val contentX = spacing.lg.px + avatarSize + spacing.md.px
    val contentWidth = (widthPx - contentX - spacing.lg.px).coerceAtLeast(1)
    val top = spacing.md.px

    val timeText = this.time?.let { TIME_PREFIX + it }
    val name: ShadowListKitTextLayout
    val handle: ShadowListKitTextLayout?
    val time: ShadowListKitTextLayout?
    val nameFrame: Rect
    val handleFrame: Rect
    val timeFrame: Rect
    var headerBottom = top + subhead.scaledLineHeightPx()
    if (ShadowListUtilsPlatform.isLargeText) {
      /*
       * At a large text size each part takes the lines it needs, and a part with no room beside
       * the one before it starts a new line.
       */
      name = subhead.with(ShadowListUtilsFontWeight.SEMIBOLD).layout(item.author.name, contentWidth)
      handle = item.author.handle?.let { subhead.layout(it, contentWidth) }
      time = timeText?.let { subhead.layout(it, contentWidth) }
      var x = 0
      var y = top
      headerBottom = top
      fun place(part: ShadowListKitTextLayout?): Rect {
        if (part == null) return Rect()
        if (x > 0 && x + part.width > contentWidth) {
          x = 0
          y = headerBottom + gap
        }
        val frame = frameOf(contentX + x, y, part)
        x += part.width + gap
        headerBottom = max(headerBottom, frame.bottom)
        return frame
      }
      nameFrame = place(name)
      handleFrame = place(handle)
      timeFrame = place(time)
    } else {
      // The age keeps its width. The name and then the handle take what is left of the line.
      time = timeText?.let { subhead.layout(it, contentWidth, 1) }
      var available = contentWidth - (time?.let { it.width + gap } ?: 0)
      name = subhead.with(ShadowListUtilsFontWeight.SEMIBOLD).layout(item.author.name, max(1, available), 1)
      available -= name.width + gap
      handle = item.author.handle?.takeIf { available >= 1 }?.let { subhead.layout(it, available, 1) }
      nameFrame = frameOf(contentX, top, name)
      handleFrame = handle?.let { frameOf(nameFrame.right + gap, top, it) } ?: Rect()
      timeFrame = time?.let { frameOf((if (handle == null) nameFrame.right else handleFrame.right) + gap, top, it) } ?: Rect()
    }

    var bottom = headerBottom + spacing.xxs.px
    var text: ShadowListKitTextLayout? = null
    var textFrame = Rect()
    item.text?.takeIf { it.isNotEmpty() }?.let { body ->
      val typeset = subhead.layout(body, contentWidth)
      textFrame = Rect(contentX, bottom, contentX + contentWidth, bottom + typeset.height)
      text = typeset
      bottom = textFrame.bottom + (if (item.images.isEmpty()) 0 else spacing.md.px)
    }

    var imageFrame = Rect()
    var stripFrame = Rect()
    var stripImageFrames = emptyList<Rect>()
    if (item.images.size == 1) {
      imageFrame = Rect(contentX, bottom, contentX + contentWidth, bottom + singleImageHeight(item.images[0], contentWidth))
      bottom = imageFrame.bottom
    } else if (item.images.size > 1) {
      // The strip starts and ends a gap outside the content, and every image has a gap on both sides.
      val imageWidth = STRIP_IMAGE_WIDTH_DP.px
      val imageHeight = ShadowListUtilsCore.FEED_SINGLE_IMAGE_HEIGHT_DP.px
      val step = imageWidth + 2 * gap
      val contentSize = item.images.size * step
      stripFrame = Rect(contentX - gap, bottom, contentX + contentWidth + gap, bottom + imageHeight)
      stripImageFrames = item.images.indices.map { index ->
        direction.place(Rect(gap + index * step, 0, gap + index * step + imageWidth, imageHeight), contentSize)
      }
      bottom = stripFrame.bottom
    }

    val avatarFrame = Rect(spacing.lg.px, top, spacing.lg.px + avatarSize, top + avatarSize)
    val height = max(bottom, avatarFrame.bottom) + spacing.md.px
    val separatorFrame = Rect(theme.rowInset.px, height - ShadowListUtilsTheme.HAIRLINE_PX, widthPx, height)
    return ShadowListUtilsFeedRowLayout(
      widthPx, height, direction.place(avatarFrame), name, direction.place(nameFrame), handle, direction.place(handleFrame),
      time, direction.place(timeFrame), text, direction.place(textFrame), direction.place(imageFrame),
      direction.place(stripFrame), stripImageFrames, direction.place(separatorFrame))
  }

  /*
   * A single image keeps its own aspect within the limits the core sets, or takes the core's
   * fixed height when it has no size.
   */
  private fun singleImageHeight(image: ShadowListUtilsFeedImage, widthPx: Int): Int {
    val frame = DoubleArray(ShadowListUtilsCore.FEED_OUT_SLOTS)
    ShadowListUtilsCore.feedSingleImageFrame((image.width ?: 0f).toDouble(), (image.height ?: 0f).toDouble(), frame)
    val aspectRatio = frame[ShadowListUtilsCore.FEED_OUT_ASPECT_RATIO]
    if (aspectRatio > 0.0) return (widthPx / aspectRatio).roundToInt()
    return frame[ShadowListUtilsCore.FEED_OUT_HEIGHT].toFloat().px
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsFeedRow"
    const val AVATAR_SIZE_DP = 40f

    /*
     * The width of an image of a strip of several. Its height is the core's height of a single
     * image without a size.
     */
    const val STRIP_IMAGE_WIDTH_DP = 280f

    /*
     * What goes between the handle and the age.
     */
    private const val TIME_PREFIX = "· "
  }
}

/*
 * stripImageFrames are in the strip's content, one per image.
 */
class ShadowListUtilsFeedRowLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val avatarFrame: Rect,
  val name: ShadowListKitTextLayout,
  val nameFrame: Rect,
  val handle: ShadowListKitTextLayout?,
  val handleFrame: Rect,
  val time: ShadowListKitTextLayout?,
  val timeFrame: Rect,
  val text: ShadowListKitTextLayout?,
  val textFrame: Rect,
  val imageFrame: Rect,
  val stripFrame: Rect,
  val stripImageFrames: List<Rect>,
  val separatorFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsFeedRowView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsFeedRow.REUSE_IDENTIFIER) {
  private val avatar = ShadowListUtilsAvatar(context, ShadowListUtilsFeedRow.AVATAR_SIZE_DP, ShadowListUtilsTheme.LIGHT)
  private val name = ShadowListKitTextView(context)
  private val handle = ShadowListKitTextView(context)
  private val time = ShadowListKitTextView(context)
  private val text = ShadowListKitTextView(context)
  private val nameBars = ShadowListUtilsRedactedBarsView(context)
  private val handleBars = ShadowListUtilsRedactedBarsView(context)
  private val textBars = ShadowListUtilsRedactedBarsView(context)
  private val image = ShadowListUtilsImageView(context)
  private val strip = HorizontalScrollView(context).apply {
    isHorizontalScrollBarEnabled = false
    clipToPadding = false
    // The frames the layout hands over are already mirrored for a right-to-left reader.
    layoutDirection = LAYOUT_DIRECTION_LTR
  }
  private val stripContent = FrameLayout(context)
  private val stripImages = ArrayList<ShadowListUtilsImageView>()
  private val separator = View(context)

  /*
   * The key of the post the strip was last placed for. The strip goes back to its start only
   * for another post.
   */
  private var stripKey: String? = null

  init {
    strip.addView(stripContent)
    for (view in listOf(avatar, name, handle, time, text, nameBars, handleBars, textBars, image, strip, separator)) {
      addView(view)
    }
  }

  override fun fill(row: ShadowListUtilsRow) {
    val feed = row as ShadowListUtilsFeedRow
    val item = feed.item
    val colors = feed.theme.colors
    val placeholder = item.placeholder
    restingBackgroundColor = colors.background
    separator.setBackgroundColor(colors.separator)
    avatar.theme = feed.theme
    if (placeholder) {
      avatar.show("", null, colors.fill)
    } else {
      avatar.show(item.author.name, item.author.avatarUrl, item.author.avatarColor)
    }
    for (bars in listOf(nameBars, handleBars, textBars)) {
      bars.color = colors.fill
      bars.visibility = if (placeholder) VISIBLE else GONE
    }
    val single = item.images.size == 1
    image.visibility = if (single) VISIBLE else GONE
    strip.visibility = if (item.images.size > 1) VISIBLE else GONE
    if (single) show(item.images[0], image, feed) else image.url = null
    fillStrip(feed)
    contentDescription = if (placeholder) {
      feed.labels.loading
    } else {
      listOfNotNull(item.author.name, item.author.handle, feed.time, item.text).joinToString(", ")
    }
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val feed = row as ShadowListUtilsFeedRow
    val frames = layout as ShadowListUtilsFeedRowLayout
    val colors = feed.theme.colors
    val placeholder = feed.item.placeholder
    avatar.place(frames.avatarFrame)
    name.show(frames.name.takeIf { !placeholder }, colors.label, frames.nameFrame)
    handle.show(frames.handle.takeIf { !placeholder }, colors.secondaryLabel, frames.handleFrame)
    time.show(frames.time.takeIf { !placeholder }, colors.secondaryLabel, frames.timeFrame)
    text.show(frames.text.takeIf { !placeholder }, colors.label, frames.textFrame)
    if (placeholder) {
      val lineHeight = feed.theme.typography.subhead.scaledLineHeightPx()
      showBars(nameBars, frames.name, frames.nameFrame, lineHeight)
      showBars(handleBars, frames.handle, frames.handleFrame, lineHeight)
      showBars(textBars, frames.text, frames.textFrame, lineHeight)
    }
    if (image.visibility == VISIBLE) image.place(frames.imageFrame)
    if (strip.visibility == VISIBLE) placeStrip(feed, frames)
    separator.place(frames.separatorFrame)
  }

  private val feedList: ShadowListUtilsFeedList? get() = enclosingList as? ShadowListUtilsFeedList

  /*
   * A placeholder shows the frame in the image's color alone.
   */
  private fun show(image: ShadowListUtilsFeedImage, view: ShadowListUtilsImageView, feed: ShadowListUtilsFeedRow) {
    val placeholder = feed.item.placeholder
    view.cornerRadius = feed.theme.radius.lg
    view.placeholderColor = image.color ?: feed.theme.colors.elevated2
    view.url = if (placeholder) null else image.url
    view.isClickable = pressesImages
    view.contentDescription = if (placeholder) null else image.alt ?: feed.labels.image
    view.importantForAccessibility = if (placeholder) IMPORTANT_FOR_ACCESSIBILITY_NO else IMPORTANT_FOR_ACCESSIBILITY_YES
  }

  /*
   * The strip of several images, each one in a view of its own.
   */
  private fun fillStrip(feed: ShadowListUtilsFeedRow) {
    val images = if (feed.item.images.size > 1) feed.item.images else emptyList()
    while (stripImages.size < images.size) {
      val view = ShadowListUtilsImageView(context)
      prepareImage(view, stripImages.size)
      stripContent.addView(view)
      stripImages.add(view)
    }
    for ((index, view) in stripImages.withIndex()) {
      view.visibility = if (index < images.size) VISIBLE else GONE
      if (index < images.size) show(images[index], view, feed) else view.url = null
    }
  }

  /*
   * Place the strip and its images. The strip opens at its start, which is its right end in a
   * layout that runs from right to left, and keeps its place while it shows the same post.
   */
  private fun placeStrip(feed: ShadowListUtilsFeedRow, frames: ShadowListUtilsFeedRowLayout) {
    val gap = feed.theme.spacing.xs.px
    val contentWidth = frames.stripImageFrames.size * (ShadowListUtilsFeedRow.STRIP_IMAGE_WIDTH_DP.px + 2 * gap)
    strip.setPadding(gap, 0, gap, 0)
    stripContent.minimumWidth = contentWidth
    for ((view, frame) in stripImages.zip(frames.stripImageFrames)) {
      val params = view.layoutParams as FrameLayout.LayoutParams
      if (params.width == frame.width() && params.height == frame.height() && params.leftMargin == frame.left) continue
      params.width = frame.width()
      params.height = frame.height()
      params.leftMargin = frame.left
      view.layoutParams = params
    }
    strip.place(frames.stripFrame)
    if (stripKey == feed.key) return
    stripKey = feed.key
    val end = (contentWidth + 2 * gap - frames.stripFrame.width()).coerceAtLeast(0)
    strip.scrollTo(if (ShadowListUtilsPlatform.isRightToLeft) end else 0, 0)
  }

  private fun showBars(view: ShadowListUtilsRedactedBarsView, layout: ShadowListKitTextLayout?, frame: Rect, lineHeightPx: Int) {
    view.bars = layout?.let { ShadowListUtilsRedactedText.bars(it, lineHeightPx) } ?: emptyList()
    view.place(frame)
  }

  // region Image presses

  private fun prepareImage(view: ShadowListUtilsImageView, index: Int) {
    view.setOnClickListener { pressImage(index) }
    view.isClickable = false
    view.accessibilityDelegate = imageAccessibility
  }

  private fun pressImage(index: Int) {
    val feed = row as? ShadowListUtilsFeedRow ?: return
    if (feed.item.placeholder) return
    feedList?.onPressImage?.invoke(feed.item, index)
  }

  /*
   * An image takes a tap only in a list that asks for image presses. Otherwise the tap is the
   * row's.
   */
  private val pressesImages: Boolean
    get() = feedList?.onPressImage != null && (row as? ShadowListUtilsFeedRow)?.item?.placeholder == false

  /*
   * A cell can be filled before it is in its list. Its images learn here whether the list
   * takes their presses.
   */
  override fun onAttachedToWindow() {
    super.onAttachedToWindow()
    val pressable = pressesImages
    image.isClickable = pressable
    for (view in stripImages) view.isClickable = pressable
  }

  override fun claimsTouch(x: Float, y: Float): Boolean {
    if (!pressesImages) return false
    return (image.visibility == VISIBLE && inside(image, x, y)) || (strip.visibility == VISIBLE && inside(strip, x, y))
  }

  private fun inside(view: View, x: Float, y: Float): Boolean =
    x >= view.left && x < view.right && y >= view.top && y < view.bottom

  // endregion

  // region Accessibility

  /*
   * The post's text reads as one element, a button in a list that selects. Each image is an
   * element with its alt text, and a button in a list that takes image presses.
   */
  private val imageAccessibility = object : AccessibilityDelegate() {
    override fun onInitializeAccessibilityNodeInfo(host: View, info: AccessibilityNodeInfo) {
      super.onInitializeAccessibilityNodeInfo(host, info)
      info.className = (if (host.isClickable) Button::class.java else ImageView::class.java).name
    }
  }

  /*
   * The single image takes the delegate above, which is declared by now.
   */
  init {
    prepareImage(image, 0)
  }

  // endregion
}
