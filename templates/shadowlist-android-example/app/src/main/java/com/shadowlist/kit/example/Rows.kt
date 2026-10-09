package com.shadowlist.kit.example

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.roundToInt

/*
 * A row of a list. Its layout is a pure function of the row and the width, safe to compute off
 * the UI thread, and a row view only copies the frames it holds.
 */
interface Row {
  val key: String
  val viewType: String
  fun makeView(context: Context): RowView
  fun layout(width: Int): RowLayout
}

/*
 * A row's frames for one width. Only the layout that built it sets the height.
 */
open class RowLayout(val width: Int, height: Int) {
  var height = height
    protected set
}

/*
 * The content of one row, shared by every list engine. The engines only differ in how they
 * host it and where its layout comes from. It is an ShadowListKitListCell, which RecyclerView hosts as
 * a plain FrameLayout.
 */
abstract class RowView(context: Context, viewType: String) : ShadowListKitListCell(context, viewType) {
  var row: Row? = null
    private set
  private var rowLayout: RowLayout? = null

  init {
    setBackgroundColor(Theme.background)
  }

  /*
   * Show a row. With a layout the frames are taken as is. Without one the layout is computed
   * on the UI thread the first time the view knows its width, like a self-sizing cell.
   */
  fun configure(row: Row, layout: RowLayout?) {
    this.row = row
    rowLayout = layout
    fill(row)
    requestLayout()
  }

  private fun ensureLayout(width: Int): RowLayout? {
    val row = row ?: return null
    if (width <= 0) return null
    rowLayout?.let { if (it.width == width) return it }
    return row.layout(width).also { rowLayout = it }
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    val height = if (MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.EXACTLY) {
      MeasureSpec.getSize(heightMeasureSpec)
    } else {
      ensureLayout(width)?.height ?: 0
    }
    setMeasuredDimension(width, height)
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    val row = row ?: return
    val layout = ensureLayout(right - left) ?: return
    apply(row, layout)
  }

  /*
   * Set the content, like colors and images, without frames.
   */
  open fun fill(row: Row) {}

  /*
   * Set the frames from the layout.
   */
  open fun apply(row: Row, layout: RowLayout) {}
}

/*
 * Measure and place a child at a frame in one step. A child that only moved keeps its measure.
 */
fun View.place(left: Int, top: Int, width: Int, height: Int) {
  if (isLayoutRequested || measuredWidth != width || measuredHeight != height) {
    measure(
      View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY))
  }
  layout(left, top, left + width, top + height)
}

fun View.place(frame: Rect) = place(frame.left, frame.top, frame.width(), frame.height())

fun ShadowListKitTextView.show(layout: ShadowListKitTextLayout?, color: Int, left: Int, top: Int) {
  textColor = color
  textLayout = layout
  visibility = if (layout == null) View.GONE else View.VISIBLE
  if (layout != null) place(left, top, layout.width, layout.height)
}

fun frameOf(left: Int, top: Int, layout: ShadowListKitTextLayout) = Rect(left, top, left + layout.width, top + layout.height)

/*
 * Precomputed layouts by row key and width, filled ahead on a background thread.
 */
class LayoutCache {
  private val layouts = ConcurrentHashMap<String, RowLayout>()
  private val executor = Executors.newSingleThreadExecutor()
  private val generation = AtomicInteger()

  // Set by the cost scenario while it times updates. Prefetch work would compete with them.
  @Volatile var prefetchPaused = false

  fun layout(row: Row, width: Int): RowLayout {
    layouts[row.key]?.let { if (it.width == width) return it }
    return row.layout(width).also { layouts[row.key] = it }
  }

  fun cached(row: Row, width: Int): RowLayout? = layouts[row.key]?.takeIf { it.width == width }

  /*
   * Lay out rows the list has not reached yet. Rows nearest the given index go first.
   */
  fun prefetch(rows: List<Row>, width: Int, around: Int = 0) {
    if (width <= 0 || prefetchPaused) return
    val current = generation.incrementAndGet()
    executor.execute {
      val ordered = rows.indices.sortedBy { abs(it - around) }
      for (index in ordered) {
        if (generation.get() != current) return@execute
        val row = rows[index]
        if (cached(row, width) == null) layout(row, width)
      }
    }
  }

  fun invalidate(key: String) {
    layouts.remove(key)
  }

  /*
   * Run work on the main thread once the prefetch queued so far finished.
   */
  fun whenIdle(work: () -> Unit) {
    val main = android.os.Handler(android.os.Looper.getMainLooper())
    executor.execute { main.post(work) }
  }
}

// region Shared pieces

/*
 * A colored circle with initials. The initials are typeset once per name and size.
 */
class AvatarView(context: Context, private val size: Int) : View(context) {
  companion object {
    private val cache = ConcurrentHashMap<String, ShadowListKitTextLayout>()
    private val styles = ConcurrentHashMap<Int, TextStyle>()

    fun initials(author: Author, size: Int): ShadowListKitTextLayout = cache.getOrPut("${author.initials}#$size") {
      val points = size / 1.dpf
      val style = styles.getOrPut(size) { TextStyle((points * 0.43f).toInt().toFloat(), true, points * 0.52f) }
      style.layout(author.initials, size)
    }
  }

  private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
  private var text: ShadowListKitTextLayout? = null

  fun show(author: Author) {
    paint.color = author.color
    text = initials(author, size)
    invalidate()
  }

  override fun onDraw(canvas: Canvas) {
    val radius = size / 2f
    canvas.drawCircle(radius, radius, radius, paint)
    val layout = text?.layout ?: return
    layout.paint.color = Theme.label
    canvas.save()
    canvas.translate(((size - text!!.width) / 2).toFloat(), ((size - text!!.height) / 2).toFloat())
    layout.draw(canvas)
    canvas.restore()
  }
}

class SeparatorView(context: Context) : View(context) {
  init {
    setBackgroundColor(Theme.separator)
  }
}

// endregion

// region Feed

class FeedRow(val post: FeedPost) : Row {
  override val key get() = post.id
  override val viewType get() = "feed"
  override fun makeView(context: Context): RowView = FeedRowView(context)

  override fun layout(width: Int): RowLayout {
    val contentX = (16 + 40 + 12).dp
    val contentWidth = width - contentX - 16.dp
    val top = 12.dp
    val date = TextStyle.subhead.layout("· ${post.time}", contentWidth, 1)
    var available = max(0, contentWidth - date.width - 8.dp)
    val name = TextStyle.subheadSemibold.layout(post.author.name, max(1, available), 1)
    available -= name.width
    val handle = TextStyle.subhead.layout(post.handle, max(1, available), 1)
    val nameFrame = frameOf(contentX, top, name)
    val handleFrame = frameOf(nameFrame.right + 4.dp, top, handle)
    val dateFrame = frameOf(handleFrame.right + 4.dp, top, date)
    val body = TextStyle.subhead.layout(post.text, contentWidth)
    val bodyFrame = frameOf(contentX, top + 22.dp, body)
    val imagesTop = bodyFrame.bottom + 12.dp
    val multi = post.images.size > 1
    val image = if (multi) Rect() else Rect(contentX, imagesTop, contentX + contentWidth, imagesTop + 200.dp)
    val carousel = if (multi) Rect(contentX - 4.dp, imagesTop, contentX + contentWidth + 4.dp, imagesTop + 200.dp) else Rect()
    return FeedLayout(width, imagesTop + 200.dp + 12.dp, name, handle, date, body, nameFrame, handleFrame, dateFrame,
      bodyFrame, image, carousel)
  }
}

class FeedLayout(
  width: Int, height: Int,
  val name: ShadowListKitTextLayout, val handle: ShadowListKitTextLayout, val date: ShadowListKitTextLayout, val body: ShadowListKitTextLayout,
  val nameFrame: Rect, val handleFrame: Rect, val dateFrame: Rect, val bodyFrame: Rect,
  val image: Rect, val carousel: Rect,
) : RowLayout(width, height)

class FeedRowView(context: Context) : RowView(context, "feed") {
  private val avatar = AvatarView(context, 40.dp)
  private val name = ShadowListKitTextView(context)
  private val handle = ShadowListKitTextView(context)
  private val date = ShadowListKitTextView(context)
  private val body = ShadowListKitTextView(context)
  private val image = RemoteImageView(context).apply { cornerRadius = 16.dpf }
  private val carousel = HorizontalScrollView(context).apply {
    isHorizontalScrollBarEnabled = false
    clipToPadding = false
  }
  private val carouselContent = FrameLayout(context)
  private val carouselImages = ArrayList<RemoteImageView>()
  private val separator = SeparatorView(context)

  init {
    carousel.addView(carouselContent)
    for (view in listOf(avatar, name, handle, date, body, image, carousel, separator)) addView(view)
  }

  override fun fill(row: Row) {
    val post = (row as FeedRow).post
    avatar.show(post.author)
    val multi = post.images.size > 1
    image.visibility = if (multi) GONE else VISIBLE
    carousel.visibility = if (multi) VISIBLE else GONE
    if (multi) fillCarousel(post.images)
  }

  private fun fillCarousel(urls: List<String>) {
    while (carouselImages.size < urls.size) {
      val view = RemoteImageView(context).apply { cornerRadius = 16.dpf }
      carouselContent.addView(view)
      carouselImages.add(view)
    }
    for ((index, view) in carouselImages.withIndex()) {
      view.visibility = if (index < urls.size) VISIBLE else GONE
      view.setImage(urls.getOrNull(index), 280.dp, 200.dp)
    }
    carousel.scrollTo(0, 0)
  }

  override fun apply(row: Row, layout: RowLayout) {
    val feed = layout as FeedLayout
    val post = (row as FeedRow).post
    avatar.place(16.dp, 12.dp, 40.dp, 40.dp)
    name.show(feed.name, Theme.label, feed.nameFrame.left, feed.nameFrame.top)
    handle.show(feed.handle, Theme.secondaryLabel, feed.handleFrame.left, feed.handleFrame.top)
    date.show(feed.date, Theme.secondaryLabel, feed.dateFrame.left, feed.dateFrame.top)
    body.show(feed.body, Theme.label, feed.bodyFrame.left, feed.bodyFrame.top)
    if (image.visibility == VISIBLE) {
      image.place(feed.image)
      image.setImage(post.images.first(), feed.image.width(), feed.image.height())
    }
    if (carousel.visibility == VISIBLE) applyCarousel(feed.carousel, post.images.size)
    separator.place(68.dp, feed.height - Theme.hairline, feed.width - 68.dp, Theme.hairline)
  }

  private fun applyCarousel(frame: Rect, count: Int) {
    carouselContent.minimumWidth = count * 288.dp + 8.dp
    for ((index, view) in carouselImages.withIndex()) {
      if (index >= count) continue
      val params = view.layoutParams as FrameLayout.LayoutParams
      params.width = 280.dp
      params.height = 200.dp
      params.leftMargin = 4.dp + index * 288.dp
    }
    carousel.place(frame)
  }
}

// endregion

// region Chat

class ChatRow(val message: ChatMessage) : Row {
  override val key get() = message.id
  override val viewType get() = "chat"
  override fun makeView(context: Context): RowView = ChatRowView(context)

  override fun layout(width: Int): RowLayout {
    val own = message.isOwn
    val contentLeft = if (own) 12.dp else (12 + 30 + 8).dp
    val layout = ChatLayout(width)
    var y = 2.dp
    if (message.images.isNotEmpty()) {
      val multi = message.images.size > 1
      val blockHeight = if (multi) 242.dp else 320.dp
      val x = if (own) width - 12.dp - 240.dp else contentLeft
      layout.images = Rect(x, y + 2.dp, x + 240.dp, y + 2.dp + (if (multi) 240.dp else 320.dp))
      y += 2.dp + blockHeight + 2.dp
    } else {
      y = layoutText(layout, width, contentLeft, y)
    }
    val height = y + 2.dp
    if (!own) layout.avatar = Rect(12.dp, height - 4.dp - 30.dp, 12.dp + 30.dp, height - 4.dp)
    layout.finish(height)
    return layout
  }

  private fun layoutText(layout: ChatLayout, width: Int, contentLeft: Int, top: Int): Int {
    val own = message.isOwn
    var y = top
    val maxColumn = (0.75f * (width - 24.dp)).toInt()
    if (!own) {
      val name = TextStyle.caption.layout(message.author.name, maxColumn, 1)
      layout.name = name
      layout.nameFrame = frameOf(contentLeft + 12.dp, y, name)
      y += 18.dp
    }
    val text = TextStyle.body.layout(message.text ?: "", maxColumn - 28.dp)
    layout.text = text
    val bubbleWidth = text.width + 28.dp
    val bubbleX = if (own) width - 12.dp - bubbleWidth else contentLeft
    layout.bubble = Rect(bubbleX, y, bubbleX + bubbleWidth, y + text.height + 16.dp)
    layout.textFrame = frameOf(bubbleX + 14.dp, y + 8.dp, text)
    return layout.bubble.bottom
  }
}

class ChatLayout(width: Int) : RowLayout(width, 0) {
  fun finish(height: Int) {
    this.height = height
  }

  var name: ShadowListKitTextLayout? = null
  var text: ShadowListKitTextLayout? = null
  var avatar = Rect()
  var nameFrame = Rect()
  var bubble = Rect()
  var textFrame = Rect()
  var images = Rect()
}

class ChatRowView(context: Context) : RowView(context, "chat") {
  private val avatar = AvatarView(context, 30.dp)
  private val name = ShadowListKitTextView(context)
  private val bubble = View(context)
  private val text = ShadowListKitTextView(context)
  private val imageBlock = FrameLayout(context)
  private val images = List(4) { RemoteImageView(context) }
  private val ownBubble = bubbleDrawable(true)
  private val otherBubble = bubbleDrawable(false)

  init {
    images.forEach { imageBlock.addView(it) }
    for (view in listOf(avatar, name, bubble, text, imageBlock)) addView(view)
  }

  /*
   * Radius 18 with a radius 5 tail corner at the bottom on the sender's side.
   */
  private fun bubbleDrawable(own: Boolean) = GradientDrawable().apply {
    val big = 18.dpf
    val small = 5.dpf
    cornerRadii = floatArrayOf(big, big, big, big, if (own) small else big, if (own) small else big,
      if (own) big else small, if (own) big else small)
    setColor(if (own) Theme.accent else Theme.elevated2)
  }

  override fun fill(row: Row) {
    val message = (row as ChatRow).message
    val own = message.isOwn
    val hasText = message.text != null
    avatar.visibility = if (own) GONE else VISIBLE
    avatar.show(message.author)
    bubble.visibility = if (hasText) VISIBLE else GONE
    bubble.background = if (own) ownBubble else otherBubble
    imageBlock.visibility = if (message.images.isEmpty()) GONE else VISIBLE
  }

  override fun apply(row: Row, layout: RowLayout) {
    val chat = layout as ChatLayout
    val message = (row as ChatRow).message
    if (avatar.visibility == VISIBLE) avatar.place(chat.avatar)
    name.show(if (message.isOwn) null else chat.name, Theme.secondaryLabel, chat.nameFrame.left, chat.nameFrame.top)
    if (bubble.visibility == VISIBLE) bubble.place(chat.bubble)
    text.show(chat.text, if (message.isOwn) Theme.onAccent else Theme.label, chat.textFrame.left, chat.textFrame.top)
    if (message.images.isNotEmpty()) applyImages(chat.images, message.images)
  }

  private fun applyImages(frame: Rect, urls: List<String>) {
    imageBlock.place(frame)
    if (urls.size > 1) {
      for ((index, view) in images.withIndex()) {
        view.visibility = VISIBLE
        view.cornerRadius = 8.dpf
        view.place((index % 2) * 121.dp, (index / 2) * 121.dp, 119.dp, 119.dp)
        view.setImage(urls[index], 119.dp, 137.dp)
      }
      return
    }
    images[0].visibility = VISIBLE
    images[0].cornerRadius = 16.dpf
    images[0].place(0, 0, 240.dp, 320.dp)
    images[0].setImage(urls[0], 240.dp, 368.dp)
    for (index in 1 until images.size) images[index].visibility = GONE
  }
}

// endregion

// region Directory

class ContactRow(val contact: Contact, val separatorBelow: Boolean) : Row {
  override val key get() = contact.id
  override val viewType get() = "contact"
  override fun makeView(context: Context): RowView = ContactRowView(context)

  override fun layout(width: Int): RowLayout {
    val textWidth = width - 68.dp - 12.dp
    return ContactLayout(width, 67.dp + (if (separatorBelow) Theme.hairline else 0),
      TextStyle.body.layout(contact.author.name, textWidth, 1),
      TextStyle.subhead.layout(contact.subtitle, textWidth, 1))
  }
}

class ContactLayout(width: Int, height: Int, val name: ShadowListKitTextLayout, val subtitle: ShadowListKitTextLayout) :
  RowLayout(width, height)

class ContactRowView(context: Context) : RowView(context, "contact") {
  private val avatar = AvatarView(context, 40.dp)
  private val name = ShadowListKitTextView(context)
  private val subtitle = ShadowListKitTextView(context)
  private val separator = SeparatorView(context)
  private val strip = SeparatorView(context)

  init {
    for (view in listOf(avatar, name, subtitle, separator, strip)) addView(view)
  }

  override fun fill(row: Row) {
    val contact = row as ContactRow
    avatar.show(contact.contact.author)
    strip.visibility = if (contact.separatorBelow) VISIBLE else GONE
  }

  override fun apply(row: Row, layout: RowLayout) {
    val contact = layout as ContactLayout
    val width = contact.width
    avatar.place(16.dp, 13.5f.dp, 40.dp, 40.dp)
    name.show(contact.name, Theme.label, 68.dp, 12.dp)
    subtitle.show(contact.subtitle, Theme.secondaryLabel, 68.dp, 35.dp)
    separator.place(68.dp, 67.dp - Theme.hairline, width - 68.dp, Theme.hairline)
    if (strip.visibility == VISIBLE) strip.place(68.dp, 67.dp, width - 68.dp, Theme.hairline)
  }
}

class SectionHeaderRow(val title: String, val count: Int) : Row {
  override val key get() = "section-$title"
  override val viewType get() = "section"
  override fun makeView(context: Context): RowView = SectionHeaderView(context)

  override fun layout(width: Int): RowLayout = SectionHeaderLayout(width, 38.dp,
    TextStyle.footnoteSemibold.layout(title.uppercase(), width / 2, 1),
    TextStyle.footnote.layout("$count", width / 2, 1))
}

class SectionHeaderLayout(width: Int, height: Int, val title: ShadowListKitTextLayout, val count: ShadowListKitTextLayout) :
  RowLayout(width, height)

class SectionHeaderView(context: Context) : RowView(context, "section") {
  private val title = ShadowListKitTextView(context)
  private val count = ShadowListKitTextView(context)

  init {
    setBackgroundColor(Theme.elevated)
    addView(title)
    addView(count)
  }

  override fun apply(row: Row, layout: RowLayout) {
    val header = layout as SectionHeaderLayout
    title.show(header.title, Theme.secondaryLabel, 16.dp, 10.dp)
    count.show(header.count, Theme.tertiaryLabel, header.width - 16.dp - header.count.width, 10.dp)
  }
}

// endregion

// region Gallery

class PhotoRow(val photo: Photo) : Row {
  override val key get() = photo.id
  override val viewType get() = "photo"
  override fun makeView(context: Context): RowView = PhotoCardView(context)

  override fun layout(width: Int): RowLayout {
    val imageWidth = width - 12.dp
    val imageHeight = (imageWidth * photo.aspect).roundToInt()
    val title = TextStyle.subhead.layout(photo.title, imageWidth - 8.dp, 2)
    return PhotoLayout(width, imageHeight + 8.dp + title.height + 12.dp,
      Rect(6.dp, 0, 6.dp + imageWidth, imageHeight), title, frameOf(10.dp, imageHeight + 8.dp, title))
  }
}

class PhotoLayout(width: Int, height: Int, val image: Rect, val title: ShadowListKitTextLayout, val titleFrame: Rect) :
  RowLayout(width, height)

class PhotoCardView(context: Context) : RowView(context, "photo") {
  private val image = RemoteImageView(context).apply { cornerRadius = 12.dpf }
  private val title = ShadowListKitTextView(context)

  init {
    addView(image)
    addView(title)
  }

  override fun apply(row: Row, layout: RowLayout) {
    val photo = layout as PhotoLayout
    image.place(photo.image)
    image.setImage((row as PhotoRow).photo.url, photo.image.width(), photo.image.height())
    title.show(photo.title, Theme.label, photo.titleFrame.left, photo.titleFrame.top)
  }
}

// endregion

/*
 * A centered line of text under the last row.
 */
class ListFooterView(context: Context, text: String) : FrameLayout(context) {
  private val label = ShadowListKitTextView(context)
  private val layout = TextStyle.footnote.layout(text, Int.MAX_VALUE / 4, 1)

  init {
    setBackgroundColor(Theme.background)
    addView(label)
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), 58.dp)
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    label.show(layout, Theme.secondaryLabel, (right - left - layout.width) / 2, 20.dp)
  }
}
