package com.shadowlist.utils

import android.graphics.Rect
import android.graphics.RectF
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.floor
import kotlin.math.roundToInt

@RunWith(AndroidJUnit4::class)
class NestedTest {
  private val theme = testTheme
  private val width = TEST_WIDTH_DP.px

  // A card along its list: the margin before it and its image.
  private val cardAlong = 16f.px + 180f.px

  private fun item(index: Int, cards: Int = 12) = ShadowListUtilsNestedItem(
    key = "shelf-$index", title = "Shelf $index",
    cards = List(cards) {
      ShadowListUtilsNestedCardItem(key = "shelf-$index-card-$it", title = "Card $it", imageUrl = "https://example.com/$it.jpg")
    })

  private fun makeList(rows: List<ShadowListUtilsNestedRow>): ShadowListUtilsList {
    val list = ShadowListUtilsList(testContext, theme)
    list.setRows(rows)
    hostNested(list)
    return list
  }

  /*
   * The rows lay out their horizontal lists in the pass after the one that mounts them.
   */
  private fun hostNested(list: ShadowListUtilsList) {
    host(list)
    host(list)
  }

  private fun nestedList(view: ShadowListUtilsRowView?): ShadowListUtilsList {
    val row = requireNotNull(view) { "the row is not mounted" }
    return (0 until row.childCount).map(row::getChildAt).filterIsInstance<ShadowListUtilsList>().single()
  }

  /*
   * The first card on screen and where its leading edge sits in the viewport. A list restored in
   * a recycled cell estimates the cards before it again, which moves its offset and not its cards.
   */
  private fun place(nested: ShadowListUtilsList): Pair<String?, Int> {
    val first = nested.visibleCells.first()
    return (first as ShadowListUtilsRowView).row?.key to first.left - nested.scrollX
  }

  @Test
  fun a_list_shows_nested_rows_with_a_horizontal_list_of_cards() = onMain {
    val list = makeList(List(6) { ShadowListUtilsNestedRow(item(it), theme) })
    assertEquals(List(6) { "shelf-$it" }, list.keys)
    val view = list.rowView(0)
    assertTrue(view is ShadowListUtilsNestedRowView)
    val nested = nestedList(view)
    assertTrue(nested.horizontal)
    assertEquals(List(12) { "shelf-0-card-$it" }, nested.keys)
    assertTrue(nested.rowView(0) is ShadowListUtilsNestedCardView)
    val listTop = theme.typography.title3.scaledLineHeightPx() + 12f.pxf
    val listHeight = (listTop + (220f + 8f).pxf + 2 * theme.typography.subhead.lineHeight.spf).roundToInt() - listTop.roundToInt()
    val second = requireNotNull(nested.rectForItem(1))
    assertEquals(RectF(cardAlong.toFloat(), 0f, 2f * cardAlong, listHeight.toFloat()), second)
    assertEquals(cardAlong, nested.rowView(0)?.width)
  }

  @Test
  fun a_changed_item_is_configured_again_and_a_deleted_item_leaves() = onMain {
    val items = List(2) { item(it) }.toMutableList()
    val list = makeList(items.map { ShadowListUtilsNestedRow(it, theme) })
    val untouched = list.rowView(0)
    items[1] = items[1].copy(cards = items[1].cards.dropLast(4))
    list.setRows(items.map { ShadowListUtilsNestedRow(it, theme) })
    hostNested(list)
    assertEquals(8, nestedList(list.rowView(1)).keys.size)
    assertEquals(12, nestedList(list.rowView(0)).keys.size)
    assertSame(untouched, list.rowView(0))
    list.setRows(listOf(ShadowListUtilsNestedRow(items[1], theme)), true)
    assertEquals(listOf("shelf-1"), list.keys)
  }

  @Test
  fun on_press_card_runs_with_the_card_and_its_row() = onMain {
    val pressed = ArrayList<String>()
    val list = makeList(listOf(ShadowListUtilsNestedRow(item(0), theme) { card, row -> pressed.add("${row.key}/${card.key}") }))
    val nested = nestedList(list.rowView(0))
    assertTrue(nested.allowsSelection)
    // The second card is the last one a phone shows whole.
    nested.tap(1)
    assertEquals(listOf("shelf-0/shelf-0-card-1"), pressed)
    assertEquals(0, nested.selectedIndices.size)

    list.setRows(listOf(ShadowListUtilsNestedRow(item(1), theme)))
    hostNested(list)
    assertFalse(nestedList(list.rowView(0)).allowsSelection)
  }

  @Test
  fun a_new_on_press_card_reaches_a_mounted_row() = onMain {
    val pressed = ArrayList<String>()
    val list = makeList(listOf(ShadowListUtilsNestedRow(item(0), theme)))
    val view = list.rowView(0)
    val nested = nestedList(view)
    assertFalse(nested.allowsSelection)
    // The same item under the same content version, with a closure the row did not have.
    val first = ShadowListUtilsNestedRow(item(0), theme) { card, _ -> pressed.add("first ${card.key}") }
    list.setRows(listOf(first))
    assertSame(view, list.rowView(0))
    assertSame(first, view?.row)
    assertTrue(nested.allowsSelection)
    nested.tap(0)
    list.setRows(listOf(ShadowListUtilsNestedRow(item(0), theme) { card, _ -> pressed.add("second ${card.key}") }))
    nested.tap(1)
    assertEquals(listOf("first shelf-0-card-0", "second shelf-0-card-1"), pressed)
  }

  @Test
  fun the_row_layout_holds_its_metrics() {
    val row = ShadowListUtilsNestedRow(item(0), theme)
    val frames = row.layout(width) as ShadowListUtilsNestedRowLayout
    val title3 = theme.typography.title3
    // The list's edges and the row's height round from sums of unrounded lengths.
    val listTop = title3.scaledLineHeightPx() + 12f.pxf
    val listBottom = listTop + (220f + 8f).pxf + 2 * theme.typography.subhead.lineHeight.spf
    assertEquals((listBottom + 16f.pxf).roundToInt(), frames.heightPx)
    assertEquals(16f.px, frames.titleFrame.left)
    assertEquals(0, frames.titleFrame.top)
    assertLines(frames.title, title3, 1)
    assertEquals(Rect(0, listTop.roundToInt(), width, listBottom.roundToInt()), frames.listFrame)
  }

  @Test
  fun the_row_grows_with_the_cards_titles_at_a_large_text_size() {
    val row = ShadowListUtilsNestedRow(item(0), theme)
    val subhead = theme.typography.subhead
    val large = withTextSize(1.5f) { row.layout(width) as ShadowListUtilsNestedRowLayout }
    val lineHeight = withTextSize(1.5f) { subhead.scaledLineHeightPx() }
    assertTrue(lineHeight > subhead.scaledLineHeightPx())
    val listTop = theme.typography.title3.let { withTextSize(1.5f) { it.scaledLineHeightPx() } } + 12f.pxf
    val listBottom = listTop + (220f + 8f).pxf + 2 * withTextSize(1.5f) { subhead.lineHeight.spf }
    assertEquals(listBottom.roundToInt() - listTop.roundToInt(), large.listFrame.height())
    assertEquals((listBottom + 16f.pxf).roundToInt(), large.heightPx)
    // Two lines of a card's title end inside the list, within the pixel their rounded line boxes add.
    val card = ShadowListUtilsNestedCard(ShadowListUtilsNestedCardItem(key = "long", title = "word ".repeat(20), imageUrl = "https://example.com/long.jpg"), theme)
    val cardLayout = withTextSize(1.5f) { card.layout(large.listFrame.height()) as ShadowListUtilsNestedCardLayout }
    assertEquals(2, cardLayout.title.lineCount)
    assertEquals(large.listFrame.height().toFloat(), cardLayout.heightPx.toFloat(), 2f)
  }

  @Test
  fun the_card_layout_holds_its_metrics() {
    val card = ShadowListUtilsNestedCard(item(0).cards[0], theme)
    val frames = card.layout(268f.px) as ShadowListUtilsNestedCardLayout
    assertEquals(cardAlong, frames.widthPx)
    assertEquals(Rect(16f.px, 0, cardAlong, 220f.px), frames.imageFrame)
    assertEquals(16f.px, frames.titleFrame.left)
    assertEquals(floor((220f + 8f).pxf).toInt(), frames.titleFrame.top)
    assertLines(frames.title, theme.typography.subhead, 2)
    // The layout's height is the height of the card's content, not the height of the list.
    assertEquals(frames.titleFrame.bottom, frames.heightPx)
  }

  @Test
  fun a_nested_list_keeps_its_scroll_position_while_its_row_is_off_screen() = onMain {
    val list = makeList(List(12) { ShadowListUtilsNestedRow(item(it), theme) })
    val nested = nestedList(list.rowView(0))
    nested.scrollToItem(5)
    hostNested(list)
    val before = place(nested)
    assertEquals("shelf-0-card-5" to 0, before)

    list.scrollToItem(10)
    hostNested(list)
    assertNull(list.rowView(0))
    assertNotNull(list.nestedAnchors["shelf-0"])
    // A row seen for the first time starts at its first card.
    assertEquals(0, nestedList(list.rowView(10)).scrollX)

    list.scrollToStart()
    hostNested(list)
    assertEquals(before, place(nestedList(list.rowView(0))))
  }

  @Test
  fun a_right_to_left_row_mirrors_its_frames() {
    val row = ShadowListUtilsNestedRow(item(0), theme)
    val plain = row.layout(width) as ShadowListUtilsNestedRowLayout
    val card = ShadowListUtilsNestedCard(item(0).cards[0], theme)
    withRightToLeft {
      val mirrored = row.layout(width) as ShadowListUtilsNestedRowLayout
      assertEquals(width - 16f.px, mirrored.titleFrame.right)
      assertEquals(width - plain.titleFrame.right, mirrored.titleFrame.left)
      assertEquals(plain.listFrame, mirrored.listFrame)
      // A card keeps its margin on the side the list starts at.
      val cardFrames = card.layout(268f.px) as ShadowListUtilsNestedCardLayout
      assertEquals(Rect(0, 0, 180f.px, 220f.px), cardFrames.imageFrame)
    }
  }
}
