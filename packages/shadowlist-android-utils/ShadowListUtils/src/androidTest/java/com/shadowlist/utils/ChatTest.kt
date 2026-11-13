package com.shadowlist.utils

import android.graphics.Rect
import android.os.Build
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityNodeInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ChatTest {
  private val theme = testTheme
  private val width = TEST_WIDTH_DP.px
  private val me = ShadowListUtilsChatAuthor(key = "me", name = "Me")
  private val ada = ShadowListUtilsChatAuthor(key = "ada", name = "Ada Lovelace")

  /*
   * Where the column of a message from someone else starts: after the padding, the avatar and
   * the gap.
   */
  private val leading = 12f.px + 30f.px + 8f.px

  private fun message(
    index: Int,
    own: Boolean = false,
    text: String? = "Hello there",
    images: List<String> = emptyList(),
    status: ShadowListUtilsChatMessageStatus? = null,
  ) = ShadowListUtilsChatMessage(
    key = "message-$index", author = if (own) me else ada, isOwn = own, text = text, images = images, status = status)

  private fun layout(item: ShadowListUtilsChatMessage, caption: String? = null, showsAuthor: Boolean = true) =
    ShadowListUtilsChatBubble(item, caption, showsAuthor, theme, ShadowListUtilsChatLabels()).layout(width) as ShadowListUtilsChatBubbleLayout

  private fun frame(view: View) = Rect(view.left, view.top, view.right, view.bottom)

  /*
   * The topmost shown part of a row view under a point in the row's coordinates, or the row view
   * itself.
   */
  private fun partAt(view: ViewGroup, x: Int, y: Int): View {
    for (position in view.childCount - 1 downTo 0) {
      val child = view.getChildAt(position)
      if (child.visibility == View.VISIBLE && frame(child).contains(x, y)) return child
    }
    return view
  }

  /*
   * A finger that lands on a point of an item's cell and has not lifted yet, then gives up.
   * The list keeps the point as its last touch.
   */
  private fun touch(list: ShadowListUtilsList, index: Int, xInCell: Int, yInCell: Int) {
    val cell = requireNotNull(list.cellForItem(index)) { "item $index is not mounted" }
    val x = (cell.left - list.scrollX + xInCell).toFloat()
    val y = (cell.top - list.scrollY + yInCell).toFloat()
    val time = SystemClock.uptimeMillis()
    for (action in intArrayOf(MotionEvent.ACTION_DOWN, MotionEvent.ACTION_CANCEL)) {
      val event = MotionEvent.obtain(time, time, action, x, y, 0)
      list.dispatchTouchEvent(event)
      event.recycle()
    }
  }

  /*
   * What a screen reader is told about a view.
   */
  private fun node(view: View): AccessibilityNodeInfo =
    AccessibilityNodeInfo.obtain().also { view.onInitializeAccessibilityNodeInfo(it) }

  /*
   * The hint a screen reader says after the label. Before API 26 a node has no hint.
   */
  private fun hint(view: View): CharSequence? =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) node(view).hintText else null

  @Test
  fun the_list_shows_its_items() = onMain {
    val list = ShadowListUtilsChatList(testContext, theme)
    host(list)
    list.items = List(6) { message(it, own = it % 2 == 0) }
    host(list)
    assertEquals(List(6) { "message-$it" }, list.keys)
    assertTrue(list.rowView(5) is ShadowListUtilsChatBubbleView)
  }

  @Test
  fun the_list_is_inverted_and_opens_at_its_end() = onMain {
    val list = ShadowListUtilsChatList(testContext, theme)
    assertTrue(list.inverted)
    assertFalse(list.followAppends)
    assertFalse(list.allowsSelection)
    assertTrue(list.showsAuthor)
    host(list)
    list.items = List(60) { message(it) }
    host(list)
    assertNotNull(list.rowView(59))
    assertNull(list.rowView(0))
  }

  @Test
  fun a_changed_item_is_configured_again_and_a_deleted_item_leaves() = onMain {
    val list = ShadowListUtilsChatList(testContext, theme)
    host(list)
    val items = MutableList(4) { message(it) }
    list.items = items.toList()
    host(list)
    val untouched = list.rowView(3)
    val untouchedRow = untouched?.row
    items[2] = items[2].copy(status = ShadowListUtilsChatMessageStatus.FAILED)
    list.setItems(items.toList(), false)
    host(list)
    assertEquals(ShadowListUtilsChatMessageStatus.FAILED, (list.rowView(2)?.row as? ShadowListUtilsChatBubble)?.item?.status)
    assertSame(list.rows[2], list.rowView(2)?.row)
    assertSame(untouched, list.rowView(3))
    assertSame(untouchedRow, list.rowView(3)?.row)
    items.removeAt(0)
    list.setItems(items.toList(), true)
    assertEquals(listOf("message-1", "message-2", "message-3"), list.keys)
  }

  @Test
  fun the_closures_run_with_their_payload() = onMain {
    val list = ShadowListUtilsChatList(testContext, theme)
    host(list)
    list.items = listOf(message(0), message(1, own = true, status = ShadowListUtilsChatMessageStatus.FAILED))
    host(list)
    val calls = ArrayList<String>()
    list.onPressItem = { calls.add("press ${it.key}") }
    list.onStartReached = { calls.add("start") }
    list.tap(0)
    list.adapter.didReachStart(list)
    assertEquals(listOf("press message-0", "start"), calls)

    val retry = R.id.shadowlist_utils_action_retry
    assertTrue(list.accessibilityActions(1).isEmpty())
    assertTrue(node(requireNotNull(list.rowView(1))).actionList.none { it.id == retry })
    list.onRetryMessage = { calls.add("retry ${it.key}") }
    host(list)
    val failed = requireNotNull(list.rowView(1))
    assertEquals(listOf("Sends the message again"), list.accessibilityActions(1).map { it.label })
    assertEquals(listOf("Sends the message again"), node(failed).actionList.filter { it.id == retry }.map { it.label.toString() })
    assertTrue(failed.performAccessibilityAction(retry, null))
    assertEquals("retry message-1", calls.last())
    assertTrue(list.accessibilityActions(0).isEmpty())
    assertTrue(node(requireNotNull(list.rowView(0))).actionList.none { it.id == retry })
  }

  @Test
  fun a_tap_on_the_failed_line_retries_and_does_not_press_the_message() = onMain {
    val list = ShadowListUtilsChatList(testContext, theme)
    host(list)
    val item = message(0, own = true, status = ShadowListUtilsChatMessageStatus.FAILED)
    list.items = listOf(item)
    host(list)
    val calls = ArrayList<String>()
    list.onPressItem = { calls.add("press ${it.key}") }
    var view = requireNotNull(list.rowView(0))
    val layout = layout(item)
    // A point beside the line, within the ring around it that takes the tap too.
    val lineX = layout.failedFrame.left - 6f.px
    val lineY = layout.failedFrame.centerY()
    val bubbleX = layout.bubbleFrame.centerX()
    val bubbleY = layout.bubbleFrame.centerY()
    // Without onRetryMessage the line belongs to the row, whose tap presses the message.
    var retry = partAt(view, lineX, lineY)
    assertNotSame(view, retry)
    assertFalse(retry.isClickable)
    assertFalse(view.claimsTouch(lineX.toFloat(), lineY.toFloat()))
    list.tap(0, lineX, lineY)
    assertEquals(listOf("press message-0"), calls)

    list.onRetryMessage = { calls.add("retry ${it.key}") }
    host(list)
    view = requireNotNull(list.rowView(0))
    retry = partAt(view, lineX, lineY)
    assertTrue(retry.isClickable)
    assertTrue(view.claimsTouch(lineX.toFloat(), lineY.toFloat()))
    assertEquals(Rect(layout.failedFrame).apply { inset(-8f.px, -8f.px) }, frame(retry))
    list.tap(0, lineX, lineY)
    assertEquals(listOf("press message-0"), calls)
    assertTrue(retry.performClick())
    assertEquals(listOf("press message-0", "retry message-0"), calls)
    // The bubble is not the line: a tap on it presses the message.
    assertNotSame(retry, partAt(view, bubbleX, bubbleY))
    assertFalse(view.claimsTouch(bubbleX.toFloat(), bubbleY.toFloat()))
    list.tap(0, bubbleX, bubbleY)
    assertEquals(listOf("press message-0", "retry message-0", "press message-0"), calls)
  }

  @Test
  fun a_hold_on_a_bubble_reports_a_long_press_and_suppresses_the_tap() = onMain {
    val list = ShadowListUtilsChatList(testContext, theme)
    host(list)
    val item = message(0, own = true)
    list.items = listOf(item)
    host(list)
    val calls = ArrayList<String>()
    list.onPressItem = { calls.add("press ${it.key}") }
    val layout = layout(item)
    // The kit tells the list's own delegate of a hold. An answer of true makes the lift no tap.
    touch(list, 0, layout.bubbleFrame.centerX(), layout.bubbleFrame.centerY())
    assertFalse(list.adapter.didLongPressItem(list, 0))
    assertEquals(emptyList<String>(), calls)
    list.onLongPressMessage = { calls.add("hold ${it.key}") }
    touch(list, 0, layout.bubbleFrame.centerX(), layout.bubbleFrame.centerY())
    assertTrue(list.adapter.didLongPressItem(list, 0))
    assertEquals(listOf("hold message-0"), calls)
    // A hold beside the bubble is no hold on the message.
    touch(list, 0, 4f.px, layout.bubbleFrame.centerY())
    assertFalse(list.adapter.didLongPressItem(list, 0))
    assertEquals(listOf("hold message-0"), calls)
    // A message has no menu.
    assertNull(list.menu(0))
  }

  @Test
  fun caption_for_item_and_shows_author_reach_the_rows() = onMain {
    val caption = theme.typography.caption
    val list = ShadowListUtilsChatList(testContext, theme)
    host(list)
    list.items = listOf(message(0), message(1))
    host(list)
    val height = requireNotNull(list.rectForItem(0)).height()
    list.captionForItem = { if (it.key == "message-0") "09:41" else null }
    host(list)
    assertEquals("09:41", (list.rowView(0)?.row as? ShadowListUtilsChatBubble)?.caption)
    assertNull((list.rows[1] as ShadowListUtilsChatBubble).caption)
    // The core's inset rounds to the pixel grid as one number, which may differ by a pixel
    // from its parts rounded one by one.
    assertEquals(height + 4f.px + caption.scaledLineHeightPx(), requireNotNull(list.rectForItem(0)).height(), 1f)
    list.showsAuthor = false
    host(list)
    assertEquals(false, (list.rowView(1)?.row as? ShadowListUtilsChatBubble)?.showsAuthor)
    assertEquals(height - caption.scaledLineHeightPx() - 2f.px, requireNotNull(list.rectForItem(1)).height(), 1f)
  }

  @Test
  fun a_text_message_is_as_tall_as_the_core_predicts() {
    val body = theme.typography.body
    val caption = theme.typography.caption
    // The theme's numbers, with the line heights text is laid out with.
    val metrics = DoubleArray(ShadowListUtilsCore.CHAT_METRIC_SLOTS)
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_XXS] = 2.0
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_XS] = 4.0
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_SM] = 8.0
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_MD] = 12.0
    metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_LG] = 16.0
    metrics[ShadowListUtilsCore.CHAT_METRIC_BODY_FONT_SIZE] = 17.0
    metrics[ShadowListUtilsCore.CHAT_METRIC_BODY_LINE_HEIGHT] = body.scaledLineHeightPx().toFloat().dp.toDouble()
    metrics[ShadowListUtilsCore.CHAT_METRIC_BODY_LETTER_SPACING] = -0.43
    metrics[ShadowListUtilsCore.CHAT_METRIC_CAPTION_LINE_HEIGHT] = caption.scaledLineHeightPx().toFloat().dp.toDouble()

    class Case(val own: Boolean, val showsAuthor: Boolean, val caption: String?, val status: ShadowListUtilsChatMessageStatus?)
    val cases = listOf(
      Case(false, true, null, null),
      Case(false, false, null, null),
      Case(true, true, "09:41", null),
      Case(false, true, "09:41", ShadowListUtilsChatMessageStatus.FAILED))
    for (case in cases) {
      val item = message(0, own = case.own, text = "word ".repeat(30), status = case.status)
      val layout = layout(item, case.caption, case.showsAuthor)
      var shape = ShadowListUtilsCore.CHAT_SHAPE_HAS_TEXT
      if (case.own) shape = shape or ShadowListUtilsCore.CHAT_SHAPE_IS_OWN
      if (case.showsAuthor) shape = shape or ShadowListUtilsCore.CHAT_SHAPE_SHOWS_AUTHOR
      if (case.caption != null) shape = shape or ShadowListUtilsCore.CHAT_SHAPE_HAS_CAPTION
      if (case.status == ShadowListUtilsChatMessageStatus.FAILED) shape = shape or ShadowListUtilsCore.CHAT_SHAPE_FAILED
      val spec = DoubleArray(ShadowListUtilsCore.CHAT_OUT_SLOTS)
      assertTrue(ShadowListUtilsCore.chatMessageSizeSpec(shape, metrics, spec))
      val text = requireNotNull(layout.text)
      assertLines(text, body)
      assertEquals(text.height + spec[ShadowListUtilsCore.CHAT_OUT_INSET_HEIGHT].toFloat().px, layout.heightPx)
      // The limits and the frames round to the pixel grid part by part, a pixel or two in all.
      val fraction = spec[ShadowListUtilsCore.CHAT_OUT_WIDTH_FRACTION].toFloat()
      assertEquals(fraction * (width - 2 * 12f.px), layout.maximumBubbleWidthPx.toFloat(), 2f)
      assertTrue(layout.bubbleFrame.width() <= layout.maximumBubbleWidthPx)
      // The frames end where the predicted height ends.
      val failedBottom = if (layout.failed == null) 0 else layout.failedFrame.top + caption.scaledLineHeightPx()
      val bottom = maxOf(layout.bubbleFrame.bottom, failedBottom)
      assertEquals(layout.heightPx.toFloat(), (bottom + 2f.px).toFloat(), 2f)
    }
  }

  @Test
  fun the_bubble_layout_holds_its_metrics() {
    val body = theme.typography.body
    val caption = theme.typography.caption
    val other = layout(message(0))
    assertEquals(0.75f * (width - 2 * 12f.px), other.maximumBubbleWidthPx.toFloat(), 2f)
    assertEquals(leading + 12f.px, other.nameFrame.left)
    assertEquals(2f.px, other.nameFrame.top)
    assertLines(other.name, caption, 1)
    assertEquals(leading, other.bubbleFrame.left)
    assertEquals(2f.px + caption.scaledLineHeightPx() + 2f.px, other.bubbleFrame.top)
    assertEquals(other.bubbleFrame.left + 14f.px, other.textFrame.left)
    assertEquals(other.bubbleFrame.top + 8f.px, other.textFrame.top)
    assertEquals(other.textFrame.width() + 2 * 14f.px, other.bubbleFrame.width())
    assertLines(other.text, body, 1)
    assertEquals(body.scaledLineHeightPx() + 2 * 8f.px, other.bubbleFrame.height())
    val avatarBottom = other.heightPx - 2 * 2f.px
    assertEquals(Rect(12f.px, avatarBottom - 30f.px, 12f.px + 30f.px, avatarBottom), other.avatarFrame)

    val own = layout(message(1, own = true))
    assertNull(own.name)
    assertEquals(width - 12f.px, own.bubbleFrame.right)
    assertEquals(2f.px, own.bubbleFrame.top)
    assertEquals(Rect(), own.avatarFrame)
    // The row is the text and the inset the core gives: the row's and the bubble's padding.
    assertEquals(body.scaledLineHeightPx() + (2f + 16f + 2f).px, own.heightPx)

    val hidden = layout(message(2), showsAuthor = false)
    assertNull(hidden.name)
    assertEquals(leading, hidden.bubbleFrame.left)
    assertEquals(2f.px, hidden.bubbleFrame.top)

    val captioned = layout(message(3, own = true), caption = "09:41")
    assertLines(captioned.caption, caption, 1)
    assertEquals(captioned.textFrame.left, captioned.captionFrame.left)
    assertEquals(captioned.textFrame.bottom + 4f.px, captioned.captionFrame.top)
    assertEquals(body.scaledLineHeightPx() + 4f.px + caption.scaledLineHeightPx() + 2 * 8f.px, captioned.bubbleFrame.height())

    val failed = layout(message(4, own = true, status = ShadowListUtilsChatMessageStatus.FAILED))
    assertLines(failed.failed, caption, 1)
    assertEquals(failed.bubbleFrame.bottom + 4f.px, failed.failedFrame.top)
    assertEquals(width - 12f.px, failed.failedFrame.right)
  }

  @Test
  fun an_empty_caption_adds_no_line() {
    val plain = layout(message(0, own = true))
    val empty = layout(message(0, own = true), caption = "")
    assertNull(empty.caption)
    assertEquals(Rect(), empty.captionFrame)
    assertEquals(plain.heightPx, empty.heightPx)
    assertEquals(plain.bubbleFrame, empty.bubbleFrame)
    val images = layout(message(1, text = null, images = listOf("a")), caption = "")
    assertNull(images.caption)
    assertEquals(2f.px + 2f.px + 320f.px + 2f.px + 2f.px, images.heightPx)
  }

  @Test
  fun a_message_without_text_or_images_keeps_its_avatar_inside_the_row() {
    val empty = layout(message(0, text = null))
    assertEquals(30f.px + 3 * 2f.px, empty.heightPx)
    assertEquals(Rect(12f.px, 2f.px, 12f.px + 30f.px, 2f.px + 30f.px), empty.avatarFrame)
    assertEquals(2f.px, layout(message(1, text = "")).avatarFrame.top)
    assertEquals(2f.px + 2f.px, layout(message(2, own = true, text = null)).heightPx)
  }

  @Test
  fun images_show_large_or_as_a_grid_of_four() {
    val single = layout(message(0, text = null, images = listOf("a")))
    assertEquals(Rect(leading, 2 * 2f.px, leading + 240f.px, 2 * 2f.px + 320f.px), single.imagesFrame)
    assertEquals(listOf(Rect(0, 0, 240f.px, 320f.px)), single.imageFrames)
    assertEquals(2f.px + 2f.px + 320f.px + 2f.px + 2f.px, single.heightPx)
    assertNull(single.name)
    assertNull(single.text)
    assertEquals(Rect(), single.bubbleFrame)

    val grid = layout(message(1, own = true, text = null, images = listOf("a", "b", "c", "d", "e")), caption = "09:41")
    val tile = 119f.px
    val gap = 2f.px
    val gridSize = 2 * tile + gap
    assertEquals(Rect(width - 12f.px - 240f.px, 2 * 2f.px, width - 12f.px, 2 * 2f.px + gridSize), grid.imagesFrame)
    assertEquals(
      listOf(
        Rect(0, 0, tile, tile), Rect(tile + gap, 0, 2 * tile + gap, tile),
        Rect(0, tile + gap, tile, 2 * tile + gap), Rect(tile + gap, tile + gap, 2 * tile + gap, 2 * tile + gap)),
      grid.imageFrames)
    assertEquals(grid.imagesFrame.bottom + 2f.px + 4f.px, grid.captionFrame.top)
    assertEquals(width - 12f.px, grid.captionFrame.right)
    assertEquals(
      2f.px + 2f.px + gridSize + 2f.px + 4f.px + theme.typography.caption.scaledLineHeightPx() + 2f.px + 2f.px, grid.heightPx)
  }

  @Test
  fun a_caption_under_images_wraps_at_the_text_message_width() {
    val long = "A caption that goes on. ".repeat(8)
    val captioned = layout(message(0, text = null, images = listOf("a")), caption = long)
    val limit = (ShadowListUtilsCore.CHAT_BUBBLE_WIDTH_FRACTION * TEST_WIDTH_DP - 46f).pxf
    assertEquals(1, captioned.caption?.lineCount)
    // The line is cut at a character, which ends it within a character of the limit.
    assertTrue(captioned.captionFrame.width() <= limit + 1f)
    assertEquals(limit, captioned.captionFrame.width().toFloat(), theme.typography.caption.fontSize.pxf)
    assertTrue(captioned.captionFrame.width() > 240f.px)
    assertEquals(leading, captioned.captionFrame.left)
    assertEquals(
      limit + 2 * ShadowListUtilsCore.CHAT_BUBBLE_PADDING_HORIZONTAL_DP.px, captioned.maximumBubbleWidthPx.toFloat(), 2f)
  }

  @Test
  fun a_right_to_left_bubble_mirrors_its_frames() {
    val own = message(0, own = true, text = null, images = listOf("a", "b"), status = ShadowListUtilsChatMessageStatus.FAILED)
    val other = message(1)
    val plainOwn = layout(own)
    val plainOther = layout(other)
    withRightToLeft {
      val mirrored = layout(own)
      assertEquals(12f.px, mirrored.imagesFrame.left)
      assertEquals(Rect(240f.px - 119f.px, 0, 240f.px, 119f.px), mirrored.imageFrames.first())
      assertEquals(width - plainOwn.failedFrame.right, mirrored.failedFrame.left)
      val mirroredOther = layout(other)
      assertEquals(width - 12f.px, mirroredOther.avatarFrame.right)
      assertEquals(width - plainOther.bubbleFrame.left, mirroredOther.bubbleFrame.right)
      assertEquals(width - plainOther.textFrame.right, mirroredOther.textFrame.left)
      assertEquals(plainOther.heightPx, mirroredOther.heightPx)
    }
  }

  @Test
  fun the_bubble_reads_its_author_text_and_state() = onMain {
    val view = ShadowListUtilsChatBubbleView(testContext)
    val labels = ShadowListUtilsChatLabels()
    fun show(item: ShadowListUtilsChatMessage, caption: String? = null) =
      view.configure(ShadowListUtilsChatBubble(item, caption, true, theme, labels), null)
    show(message(0), caption = "09:41")
    assertEquals("Ada Lovelace, Hello there, 09:41", view.contentDescription)
    show(message(0), caption = "")
    assertEquals("Ada Lovelace, Hello there", view.contentDescription)
    assertNull(hint(view))
    show(message(1, own = true, status = ShadowListUtilsChatMessageStatus.SENDING))
    assertEquals("Hello there", view.contentDescription)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) assertEquals("Sending", hint(view))
    show(message(2, own = true, text = null, images = listOf("a", "b"), status = ShadowListUtilsChatMessageStatus.FAILED))
    assertEquals("Image, Image, Not delivered. Tap to retry.", view.contentDescription)
  }
}
