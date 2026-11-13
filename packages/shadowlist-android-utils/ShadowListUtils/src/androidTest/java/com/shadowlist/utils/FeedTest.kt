package com.shadowlist.utils

import android.graphics.Rect
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Button
import android.widget.HorizontalScrollView
import android.widget.ImageView
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.roundToInt

@RunWith(AndroidJUnit4::class)
class FeedTest {
  private val theme = testTheme
  private val width = TEST_WIDTH_DP.px

  /*
   * Where a row's content starts, after the padding, the avatar and the gap, and how wide it is.
   */
  private val contentX = 16f.px + 40f.px + 12f.px
  private val contentWidth = width - contentX - 16f.px

  /*
   * How far the images of a strip are apart: an image and the gap at both of its sides.
   */
  private val step = 280f.px + 2 * 4f.px

  private fun item(index: Int, text: String? = "A short post.", images: List<ShadowListUtilsFeedImage> = emptyList()) =
    ShadowListUtilsFeedItem(
      key = "post-$index",
      author = ShadowListUtilsFeedAuthor(name = "Ada Lovelace", handle = "@ada"),
      text = text,
      images = images,
      createdAtMs = System.currentTimeMillis() - 3 * 3_600_000.0)

  private fun images(count: Int) = List(count) { ShadowListUtilsFeedImage(url = "https://example.com/$it.jpg") }

  private fun row(item: ShadowListUtilsFeedItem) = ShadowListUtilsFeedRow(item, theme, ShadowListUtilsFeedLabels())

  private fun layout(item: ShadowListUtilsFeedItem) = row(item).layout(width) as ShadowListUtilsFeedRowLayout

  private fun children(view: ViewGroup): List<View> = List(view.childCount) { view.getChildAt(it) }

  private fun frame(view: View) = Rect(view.left, view.top, view.right, view.bottom)

  /*
   * The parts of a row view a test reaches through the view tree: the strip of several images,
   * the content the strip scrolls, the images the strip shows and the single image.
   */
  private fun strip(view: ShadowListUtilsRowView): HorizontalScrollView =
    children(view).filterIsInstance<HorizontalScrollView>().first()

  private fun stripContent(view: ShadowListUtilsRowView): ViewGroup = strip(view).getChildAt(0) as ViewGroup

  private fun stripImages(view: ShadowListUtilsRowView): List<ShadowListUtilsImageView> =
    children(stripContent(view)).filterIsInstance<ShadowListUtilsImageView>().filter { it.visibility == View.VISIBLE }

  private fun singleImage(view: ShadowListUtilsRowView): ShadowListUtilsImageView =
    children(view).filterIsInstance<ShadowListUtilsImageView>().first()

  /*
   * What a screen reader is told about a view.
   */
  private fun node(view: View): AccessibilityNodeInfo =
    AccessibilityNodeInfo.obtain().also { view.onInitializeAccessibilityNodeInfo(it) }

  @Test
  fun the_list_shows_its_items() = onMain {
    val list = ShadowListUtilsFeedList(testContext, theme)
    host(list)
    list.items = List(5) { item(it) }
    host(list)
    assertEquals(List(5) { "post-$it" }, list.keys)
    assertTrue(list.rowView(0) is ShadowListUtilsFeedRowView)
    assertEquals(1, list.numberOfColumns)
    assertFalse(list.allowsSelection)
  }

  @Test
  fun a_changed_item_is_configured_again_and_a_deleted_item_leaves() = onMain {
    val list = ShadowListUtilsFeedList(testContext, theme)
    host(list)
    val items = MutableList(4) { item(it) }
    list.items = items.toList()
    host(list)
    val untouched = list.rowView(0)
    val untouchedRow = untouched?.row
    val height = requireNotNull(list.rectForItem(1)).height()
    items[1] = items[1].copy(text = "A much longer post. ".repeat(12))
    list.setItems(items.toList(), false)
    host(list)
    assertEquals(items[1].text, (list.rowView(1)?.row as? ShadowListUtilsFeedRow)?.item?.text)
    assertSame(list.rows[1], list.rowView(1)?.row)
    assertTrue(requireNotNull(list.rectForItem(1)).height() > height)
    assertSame(untouched, list.rowView(0))
    assertSame(untouchedRow, list.rowView(0)?.row)
    items.removeAt(0)
    list.setItems(items.toList(), true)
    host(list)
    assertEquals(listOf("post-1", "post-2", "post-3"), list.keys)
  }

  @Test
  fun the_closures_run_with_their_payload() = onMain {
    val list = ShadowListUtilsFeedList(testContext, theme)
    host(list)
    list.items = listOf(item(0), item(1, images = images(1)), item(2))
    val pressed = ArrayList<String>()
    val pressedImages = ArrayList<Pair<String, Int>>()
    var ends = 0
    list.onPressItem = { pressed.add(it.key) }
    list.onPressImage = { item, index -> pressedImages.add(item.key to index) }
    list.onEndReached = { ++ends }
    host(list)
    val frames = layout(list.items[1])
    list.tap(1, frames.textFrame.centerX(), frames.textFrame.centerY())
    // The layout above reached the end of these three posts already.
    ends = 0
    list.adapter.didReachEnd(list)
    assertEquals(listOf("post-1"), pressed)
    assertEquals(1, ends)
    // The image takes its own tap, and the post is not pressed with it.
    list.tap(1, frames.imageFrame.centerX(), frames.imageFrame.centerY())
    assertEquals(listOf("post-1"), pressed)
    assertTrue(singleImage(requireNotNull(list.rowView(1))).performClick())
    assertEquals(listOf("post-1" to 0), pressedImages)
  }

  @Test
  fun each_image_is_an_accessibility_element_and_a_button_with_on_press_image() = onMain {
    val list = ShadowListUtilsFeedList(testContext, theme)
    host(list)
    val images = listOf(
      ShadowListUtilsFeedImage(url = "https://example.com/a.jpg", alt = "Harbor"),
      ShadowListUtilsFeedImage(url = "https://example.com/b.jpg"))
    list.items = listOf(item(0, images = images))
    host(list)
    var view = requireNotNull(list.rowView(0))
    assertEquals("Ada Lovelace, @ada, 3h, A short post.", view.contentDescription)
    assertNotEquals(Button::class.java.name, node(view).className)
    assertEquals(listOf("Harbor", "Image"), stripImages(view).map { it.contentDescription })
    for (image in stripImages(view)) {
      assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_YES, image.importantForAccessibility)
      assertEquals(ImageView::class.java.name, node(image).className)
      assertFalse(image.isClickable)
    }
    val frames = layout(list.items[0])
    val stripX = frames.stripFrame.centerX().toFloat()
    val stripY = frames.stripFrame.centerY().toFloat()
    assertFalse(view.claimsTouch(stripX, stripY))

    list.onPressImage = { _, _ -> }
    list.onPressItem = { }
    host(list)
    view = requireNotNull(list.rowView(0))
    assertEquals(Button::class.java.name, node(view).className)
    assertEquals(listOf("Harbor", "Image"), stripImages(view).map { it.contentDescription })
    for (image in stripImages(view)) {
      assertEquals(Button::class.java.name, node(image).className)
      assertTrue(image.isClickable)
    }
    // The row's press ends where the images start, which keeps it off them.
    assertTrue(view.claimsTouch(stripX, stripY))
    assertFalse(view.claimsTouch(frames.textFrame.centerX().toFloat(), frames.textFrame.centerY().toFloat()))
  }

  @Test
  fun format_time_writes_the_age_in_place_of_the_labels() {
    assertEquals("3h", row(item(0)).time)
    assertEquals("· 3h", layout(item(0)).time?.layout?.text?.toString())
    val formatted = ShadowListUtilsFeedRow(item(0), theme, ShadowListUtilsFeedLabels()) { "earlier" }
    assertEquals("earlier", formatted.time)
    assertEquals("· earlier", (formatted.layout(width) as ShadowListUtilsFeedRowLayout).time?.layout?.text?.toString())
    val labels = ShadowListUtilsFeedLabels(time = ShadowListUtilsRelativeTimeLabels(hours = { "$it hours" }))
    assertEquals("3 hours", ShadowListUtilsFeedRow(item(0), theme, labels).time)
    val dateless = item(0).copy(createdAtMs = null)
    assertNull(row(dateless).time)
    assertNull(layout(dateless).time)
  }

  @Test
  fun an_append_keeps_the_rows_of_the_items_before() = onMain {
    val list = ShadowListUtilsFeedList(testContext, theme)
    host(list)
    val made = ArrayList<String>()
    list.rowForItem = { item ->
      made.add(item.key)
      ShadowListUtilsFeedRow(item, theme, ShadowListUtilsFeedLabels())
    }
    val items = MutableList(3) { item(it) }
    list.items = items.toList()
    host(list)
    val first = list.rowView(0)
    made.clear()
    items.add(item(3))
    list.setItems(items.toList(), false)
    host(list)
    assertEquals(listOf("post-3"), made)
    assertEquals(listOf("post-0", "post-1", "post-2", "post-3"), list.keys)
    assertSame(first, list.rowView(0))
    assertTrue(list.rowView(3) is ShadowListUtilsFeedRowView)
    made.clear()
    items[1] = items[1].copy(text = "Edited.")
    items.add(0, item(9))
    list.setItems(items.toList(), false)
    assertEquals(listOf("post-9", "post-1"), made)
  }

  @Test
  fun the_row_layout_holds_its_metrics() {
    val subhead = theme.typography.subhead
    val layout = layout(item(0))
    assertEquals(Rect(16f.px, 12f.px, 16f.px + 40f.px, 12f.px + 40f.px), layout.avatarFrame)
    assertEquals(contentX, layout.nameFrame.left)
    assertEquals(12f.px, layout.nameFrame.top)
    assertLines(layout.name, subhead, 1)
    assertEquals(layout.nameFrame.right + 4f.px, layout.handleFrame.left)
    assertEquals(layout.handleFrame.right + 4f.px, layout.timeFrame.left)
    assertEquals(contentX, layout.textFrame.left)
    assertEquals(12f.px + subhead.scaledLineHeightPx() + 2f.px, layout.textFrame.top)
    assertEquals(contentWidth, layout.textFrame.width())
    assertLines(layout.text, subhead)
    assertEquals(layout.textFrame.bottom + 12f.px, layout.heightPx)
    assertEquals(
      Rect(theme.rowInset.px, layout.heightPx - ShadowListUtilsTheme.HAIRLINE_PX, width, layout.heightPx),
      layout.separatorFrame)
    assertEquals(Rect(), layout.imageFrame)
    assertEquals(Rect(), layout.stripFrame)

    val long = layout(item(1, text = "word ".repeat(80)))
    assertLines(long.text, subhead)
    assertEquals(long.textFrame.bottom + 12f.px, long.heightPx)
  }

  @Test
  fun the_header_line_wraps_at_a_large_text_size() {
    val post = item(0).let {
      it.copy(author = it.author.copy(name = "A name long enough to take the line at a large text size"))
    }
    val subhead = theme.typography.subhead
    val plain = layout(post)
    assertEquals(1, plain.name.lineCount)
    assertEquals(plain.nameFrame.top, plain.timeFrame.top)
    assertEquals(12f.px + subhead.scaledLineHeightPx() + 2f.px, plain.textFrame.top)
    val large = withTextSize(1.5f) { layout(post) }
    assertTrue(large.name.lineCount > 1)
    assertEquals(contentX, large.nameFrame.left)
    assertEquals(12f.px, large.nameFrame.top)
    assertEquals(contentX, large.handleFrame.left)
    assertEquals(large.nameFrame.bottom + 4f.px, large.handleFrame.top)
    assertEquals(large.handleFrame.top, large.timeFrame.top)
    assertEquals(large.handleFrame.right + 4f.px, large.timeFrame.left)
    assertTrue(large.nameFrame.right <= width - 16f.px)
    assertEquals(large.handleFrame.bottom + 2f.px, large.textFrame.top)
  }

  @Test
  fun a_post_without_text_or_images_is_as_tall_as_its_avatar() {
    assertEquals(12f.px + 40f.px + 12f.px, layout(item(2, text = null)).heightPx)
    assertEquals(12f.px + 40f.px + 12f.px, layout(item(3, text = "")).heightPx)
  }

  @Test
  fun a_single_image_keeps_its_aspect_between_its_bounds() {
    fun imageFrame(image: ShadowListUtilsFeedImage) = layout(item(0, images = listOf(image))).imageFrame
    val subhead = theme.typography.subhead
    val top = 12f.px + subhead.scaledLineHeightPx() + 2f.px + subhead.scaledLineHeightPx() + 12f.px
    assertEquals(
      Rect(contentX, top, contentX + contentWidth, top + contentWidth),
      imageFrame(ShadowListUtilsFeedImage(url = "a", width = 1000f, height = 1000f)))
    // Taller than 3:4 and wider than 16:9 are kept at those limits.
    assertEquals(
      (contentWidth / 0.75).roundToInt(),
      imageFrame(ShadowListUtilsFeedImage(url = "a", width = 600f, height = 1600f)).height())
    assertEquals(
      (contentWidth / (16.0 / 9.0)).roundToInt(),
      imageFrame(ShadowListUtilsFeedImage(url = "a", width = 3000f, height = 500f)).height())
    val plain = layout(item(0, images = listOf(ShadowListUtilsFeedImage(url = "a"))))
    assertEquals(200f.px, plain.imageFrame.height())
    assertEquals(ShadowListUtilsCore.FEED_SINGLE_IMAGE_HEIGHT_DP.px, plain.imageFrame.height())
    assertEquals(plain.textFrame.bottom + 12f.px, plain.imageFrame.top)
    assertEquals(plain.imageFrame.bottom + 12f.px, plain.heightPx)
  }

  @Test
  fun several_images_show_in_a_strip_that_bleeds_past_the_content() = onMain {
    val post = item(0, images = images(3))
    val layout = layout(post)
    assertEquals(contentX - 4f.px, layout.stripFrame.left)
    assertEquals(contentWidth + 2 * 4f.px, layout.stripFrame.width())
    assertEquals(200f.px, layout.stripFrame.height())
    assertEquals(Rect(), layout.imageFrame)
    assertEquals(layout.stripFrame.bottom + 12f.px, layout.heightPx)
    assertEquals(List(3) { Rect(4f.px + it * step, 0, 4f.px + it * step + 280f.px, 200f.px) }, layout.stripImageFrames)
    val view = ShadowListUtilsFeedRowView(testContext)
    view.configure(row(post), layout)
    host(view, width, layout.heightPx)
    // The strip clips at its frame, a gap past the content on both sides.
    val strip = strip(view)
    assertEquals(layout.stripFrame, frame(strip))
    assertEquals(layout.stripImageFrames, stripImages(view).map(::frame))
    assertEquals(3 * step, stripContent(view).width)
    assertEquals(4f.px, strip.paddingLeft)
    assertEquals(4f.px, strip.paddingRight)
    assertFalse(strip.clipToPadding)
    assertEquals(0, strip.scrollX)
  }

  @Test
  fun a_strip_keeps_its_scroll_position_until_its_key_changes() = onMain {
    val images = images(4)
    val post = item(0, images = images)
    val view = ShadowListUtilsFeedRowView(testContext)
    view.configure(row(post), null)
    host(view, width, 400f.px)
    val strip = strip(view)
    strip.scrollTo(300f.px, 0)
    assertEquals(300f.px, strip.scrollX)
    // The same post with a new text, like a later age, keeps the strip where it is.
    view.configure(row(post.copy(text = "Edited.")), null)
    host(view, width, 400f.px)
    assertEquals(300f.px, strip.scrollX)
    view.configure(row(item(1, images = images)), null)
    host(view, width, 400f.px)
    assertEquals(0, strip.scrollX)
  }

  @Test
  fun a_placeholder_row_reads_as_loading_and_keeps_the_rows_height() = onMain {
    val loading = item(
      0, text = "word ".repeat(30), images = listOf(ShadowListUtilsFeedImage(url = "a", width = 400f, height = 300f)))
    val height = layout(loading).heightPx
    val placeholder = row(loading.copy(placeholder = true))
    assertNull(placeholder.time)
    val frames = placeholder.layout(width) as ShadowListUtilsFeedRowLayout
    assertNull(frames.time)
    assertEquals(height, frames.heightPx)
    val view = ShadowListUtilsFeedRowView(testContext)
    view.configure(placeholder, null)
    assertEquals("Loading post", view.contentDescription)
    // A placeholder's image is a colored frame a screen reader passes over.
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, singleImage(view).importantForAccessibility)
    assertNull(singleImage(view).url)
    view.configure(row(item(1)), null)
    assertEquals("Ada Lovelace, @ada, 3h, A short post.", view.contentDescription)
  }

  @Test
  fun a_right_to_left_row_mirrors_its_frames() {
    val post = row(item(0, images = images(3)))
    val plain = post.layout(width) as ShadowListUtilsFeedRowLayout
    val mirrored = withRightToLeft { post.layout(width) as ShadowListUtilsFeedRowLayout }
    assertEquals(width - plain.avatarFrame.right, mirrored.avatarFrame.left)
    assertEquals(width - plain.nameFrame.right, mirrored.nameFrame.left)
    assertEquals(width - plain.timeFrame.left, mirrored.timeFrame.right)
    assertEquals(width - plain.separatorFrame.left, mirrored.separatorFrame.right)
    assertEquals(width - plain.stripFrame.right, mirrored.stripFrame.left)
    assertEquals(3 * step - 4f.px, mirrored.stripImageFrames.first().right)
    assertEquals(plain.heightPx, mirrored.heightPx)
  }
}
