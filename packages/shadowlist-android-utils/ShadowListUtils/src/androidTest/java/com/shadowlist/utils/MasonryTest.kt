package com.shadowlist.utils

import android.graphics.RectF
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class MasonryTest {
  private val theme = testTheme
  private val labels = ShadowListUtilsMasonryLabels()

  // The width of one of three columns of a phone.
  private val cardWidth = 130f.px

  private fun item(index: Int, title: String? = "Photo title") = ShadowListUtilsMasonryItem(
    key = "photo-$index", image = ShadowListUtilsMasonryImage(url = "https://example.com/$index.jpg", width = 400f, height = 600f),
    title = title)

  private fun makeList(count: Int): ShadowListUtilsMasonryList {
    val list = ShadowListUtilsMasonryList(testContext, theme)
    list.items = List(count) { item(it) }
    host(list)
    return list
  }

  private fun layout(card: ShadowListUtilsMasonryCard) = card.layout(cardWidth) as ShadowListUtilsMasonryCardLayout

  private fun ShadowListUtilsList.frame(index: Int): RectF = requireNotNull(rectForItem(index)) { "item $index has no frame" }

  @Test
  fun the_list_shows_its_items() = onMain {
    val list = makeList(6)
    assertEquals(List(6) { "photo-$it" }, list.keys)
    assertTrue(list.rowView(0) is ShadowListUtilsMasonryCardView)
    assertEquals(6, list.items.size)
  }

  @Test
  fun the_list_is_a_grid_of_three_columns_that_stack_their_own_cards() = onMain {
    val list = ShadowListUtilsMasonryList(testContext, theme)
    assertEquals(3, list.numberOfColumns)
    assertFalse(list.allowsSelection)
    list.items = List(6) { item(it) }
    host(list)
    val column = TEST_WIDTH_DP.px / 3f
    assertEquals(column, list.frame(0).width(), 1f)
    assertEquals(column, list.frame(1).left, 1f)
    assertEquals(list.frame(0).top, list.frame(1).top, 0f)
    assertEquals(list.frame(0).bottom, list.frame(3).top, 0f)
  }

  @Test
  fun a_changed_item_is_configured_again_and_a_deleted_item_leaves() = onMain {
    val list = makeList(6)
    val items = list.items.toMutableList()
    val untouched = list.rowView(0)
    val untouchedRow = untouched?.row
    items[1] = items[1].copy(title = "A new title")
    list.setItems(items, false)
    host(list)
    assertEquals("A new title", (list.rowView(1)?.row as? ShadowListUtilsMasonryCard)?.item?.title)
    assertSame(list.rows[1], list.rowView(1)?.row)
    assertSame(untouched, list.rowView(0))
    // An item that equals the one before keeps its row.
    assertSame(untouchedRow, list.rowView(0)?.row)
    items.removeAt(2)
    list.setItems(items, true)
    host(list)
    assertEquals(listOf("photo-0", "photo-1", "photo-3", "photo-4", "photo-5"), list.keys)
    assertEquals(5, list.visibleCells.size)
    assertFalse(list.visibleCells.any { (it as? ShadowListUtilsRowView)?.row?.key == "photo-2" })
  }

  @Test
  fun on_press_item_runs_for_a_tapped_card_and_leaves_nothing_selected() = onMain {
    val list = makeList(3)
    val pressed = ArrayList<String>()
    var selectedWhilePressed = emptyList<Int>()
    list.onPressItem = {
      pressed.add(it.key)
      selectedWhilePressed = list.selectedIndices.toList()
    }
    assertTrue(list.allowsSelection)
    // The kit selects the item, then tells the list. The press runs before the item deselects.
    list.tap(2)
    assertEquals(listOf("photo-2"), pressed)
    assertEquals(listOf(2), selectedWhilePressed)
    assertEquals(0, list.selectedIndices.size)
    list.onPressItem = null
    assertFalse(list.allowsSelection)
  }

  @Test
  fun row_for_item_replaces_the_card() = onMain {
    val list = makeList(3)
    list.rowForItem = { CountingRow(it.key) }
    host(list)
    assertTrue(list.rowView(0) is CountingRowView)
    assertEquals(44f.px.toFloat(), list.frame(0).height(), 0f)
    list.rowForItem = null
    host(list)
    assertTrue(list.rowView(0) is ShadowListUtilsMasonryCardView)
  }

  @Test
  fun the_card_layout_holds_its_metrics() {
    val frames = layout(ShadowListUtilsMasonryCard(item(0), theme, labels))
    val imageWidth = cardWidth - 2 * 6f.px
    assertEquals(6f.px, frames.imageFrame.left)
    assertEquals(0, frames.imageFrame.top)
    assertEquals(imageWidth, frames.imageFrame.width())
    // The image's height keeps its aspect, on the pixel grid.
    assertEquals(imageWidth * 600f / 400f, frames.imageFrame.height().toFloat(), 0.5f)
    val imageHeight = frames.imageFrame.height()
    assertEquals(6f.px + 4f.px, frames.titleFrame.left)
    assertEquals(imageHeight + 8f.px, frames.titleFrame.top)
    val title = requireNotNull(frames.title)
    assertLines(title, theme.typography.subhead, 2)
    assertEquals(imageHeight + 8f.px + title.height + 12f.px, frames.heightPx)
    assertEquals(cardWidth, frames.widthPx)

    val untitled = layout(ShadowListUtilsMasonryCard(item(1, title = null), theme, labels))
    assertNull(untitled.title)
    assertTrue(untitled.titleFrame.isEmpty)
    assertEquals(imageHeight + 8f.px + 12f.px, untitled.heightPx)
    val square = item(2, title = null).let { it.copy(image = it.image.copy(height = 0f)) }
    val squareFrames = layout(ShadowListUtilsMasonryCard(square, theme, labels))
    assertEquals(imageWidth, squareFrames.imageFrame.width())
    assertEquals(imageWidth, squareFrames.imageFrame.height())
  }

  @Test
  fun a_title_takes_four_lines_at_a_large_text_size() {
    val card = ShadowListUtilsMasonryCard(item(0, title = "A long title. ".repeat(20)), theme, labels)
    assertEquals(2, layout(card).title?.lineCount)
    assertEquals(2, withTextSize(1.29f) { layout(card).title?.lineCount })
    withTextSize(1.3f) {
      val title = layout(card).title
      assertEquals(4, title?.lineCount)
      assertLines(title, theme.typography.subhead, 4)
    }
  }

  @Test
  fun a_right_to_left_card_mirrors_its_frames() {
    val card = ShadowListUtilsMasonryCard(item(0, title = "Hi"), theme, labels)
    val plain = layout(card)
    val mirrored = withRightToLeft { layout(card) }
    assertEquals(plain.imageFrame, mirrored.imageFrame)
    assertEquals(cardWidth - 6f.px - 4f.px, mirrored.titleFrame.right)
    assertEquals(cardWidth - plain.titleFrame.right, mirrored.titleFrame.left)
  }

  @Test
  fun the_card_reads_its_alt_its_title_or_the_label() = onMain {
    val view = ShadowListUtilsMasonryCardView(testContext)
    assertEquals("Photo", labels.image)
    val named = labels.copy(image = "Picture")
    var photo = item(0, title = null)
    view.configure(ShadowListUtilsMasonryCard(photo, theme, named), null)
    assertEquals("Picture", view.contentDescription)
    photo = photo.copy(title = "Harbor")
    view.configure(ShadowListUtilsMasonryCard(photo, theme, named), null)
    assertEquals("Harbor", view.contentDescription)
    photo = photo.copy(image = photo.image.copy(alt = "Boats at dusk"))
    view.configure(ShadowListUtilsMasonryCard(photo, theme, named), null)
    assertEquals("Boats at dusk", view.contentDescription)
  }
}
