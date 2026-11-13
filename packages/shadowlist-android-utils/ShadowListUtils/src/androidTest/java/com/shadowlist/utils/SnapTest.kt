package com.shadowlist.utils

import android.graphics.Rect
import android.graphics.RectF
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.shadowlist.kit.ShadowListKitSnapAlignment
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.roundToInt

@RunWith(AndroidJUnit4::class)
class SnapTest {
  private val theme = testTheme
  private val labels = ShadowListUtilsSnapLabels()
  private val width = TEST_WIDTH_DP.px

  private fun item(index: Int) = ShadowListUtilsSnapItem(
    key = "card-$index", title = "Lisbon", subtitle = "Portugal", imageUrl = "https://example.com/$index.jpg")

  private fun makeList(count: Int, cardHeight: Float? = null): ShadowListUtilsSnapList {
    val list = ShadowListUtilsSnapList(testContext, theme)
    list.cardHeight = cardHeight
    list.items = List(count, ::item)
    host(list)
    return list
  }

  private fun layout(card: ShadowListUtilsSnapCard) = card.layout(width) as ShadowListUtilsSnapCardLayout

  private fun ShadowListUtilsList.frame(index: Int): RectF = requireNotNull(rectForItem(index)) { "item $index has no frame" }

  @Test
  fun the_list_shows_its_items() = onMain {
    val list = makeList(6)
    assertEquals(List(6) { "card-$it" }, list.keys)
    assertTrue(list.rowView(0) is ShadowListUtilsSnapCardView)
  }

  @Test
  fun the_list_snaps_to_the_start_of_an_item() = onMain {
    val list = ShadowListUtilsSnapList(testContext, theme)
    assertTrue(list.snapToItem)
    assertEquals(ShadowListKitSnapAlignment.START, list.snapAlignment)
    assertFalse(list.allowsSelection)
  }

  @Test
  fun a_changed_item_is_configured_again_and_a_deleted_item_leaves() = onMain {
    val list = makeList(3, cardHeight = 200f)
    val items = list.items.toMutableList()
    val untouched = list.rowView(0)
    val untouchedRow = untouched?.row
    items[1] = items[1].copy(title = "Porto")
    list.setItems(items, false)
    host(list)
    assertEquals("Porto", (list.rowView(1)?.row as? ShadowListUtilsSnapCard)?.item?.title)
    assertSame(list.rows[1], list.rowView(1)?.row)
    assertSame(untouched, list.rowView(0))
    // An item that equals the one before keeps its row.
    assertSame(untouchedRow, list.rowView(0)?.row)
    items.removeAt(0)
    list.setItems(items, true)
    assertEquals(listOf("card-1", "card-2"), list.keys)
    host(list)
    assertEquals("card-1", list.rowView(0)?.row?.key)
  }

  @Test
  fun on_press_item_runs_for_a_tapped_card() = onMain {
    val list = makeList(3, cardHeight = 200f)
    val pressed = ArrayList<String>()
    list.onPressItem = { pressed.add(it.key) }
    assertTrue(list.allowsSelection)
    list.tap(0)
    assertEquals(listOf("card-0"), pressed)
    assertEquals(0, list.selectedIndices.size)
    list.onPressItem = null
    assertFalse(list.allowsSelection)
  }

  @Test
  fun a_card_is_a_quarter_of_the_window_unless_card_height_is_set() = onMain {
    // A list outside a window takes the height of the window the library last saw, or the screen's.
    val quarter = (ShadowListUtilsPlatform.windowHeightPx / 4f).roundToInt()
    val list = makeList(3)
    assertEquals(quarter.toFloat(), list.frame(0).height(), 0f)
    assertEquals(quarter, (list.rows.first() as? ShadowListUtilsSnapCard)?.height?.px)
    list.cardHeight = 300f
    host(list)
    assertEquals(RectF(0f, 300f.px.toFloat(), width.toFloat(), 2 * 300f.px.toFloat()), list.frame(1))
    // A card outside a snap list falls back to the height of the window the library last saw.
    val alone = ShadowListUtilsSnapCard(item(0), null, theme, labels)
    assertEquals(quarter, alone.layout(width).heightPx)
    try {
      // A window of another height sizes the cards on the pixel grid.
      ShadowListUtilsPlatform.windowHeightPx = 1001
      assertEquals(250, alone.layout(width).heightPx)
      assertEquals(250f, makeList(3).frame(0).height(), 0f)
    } finally {
      ShadowListUtilsPlatform.windowHeightPx = 0
    }
  }

  @Test
  fun a_new_card_height_lays_every_card_out_again() = onMain {
    val list = makeList(3, cardHeight = 240f)
    assertEquals(2 * 240f.px.toFloat(), list.frame(2).top, 0f)
    list.cardHeight = 120f
    host(list)
    assertEquals(RectF(0f, 2 * 120f.px.toFloat(), width.toFloat(), 3 * 120f.px.toFloat()), list.frame(2))
    assertEquals(120f.px, list.rowView(0)?.height)
    assertEquals(120f.px, list.rowView(1)?.top)
  }

  @Test
  fun the_card_layout_holds_its_metrics() {
    val frames = layout(ShadowListUtilsSnapCard(item(0), 240f, theme, labels))
    assertEquals(240f.px, frames.heightPx)
    val card = Rect(8f.px, 8f.px, width - 8f.px, 240f.px - 8f.px)
    assertEquals(card, frames.cardFrame)
    val title = requireNotNull(frames.title)
    val subtitle = requireNotNull(frames.subtitle)
    assertLines(title, theme.typography.title3, 2)
    assertLines(subtitle, theme.typography.subhead, 2)
    val captionHeight = 16f.px + title.height + subtitle.height + 16f.px
    assertEquals(Rect(0, card.height() - captionHeight, card.width(), card.height()), frames.captionFrame)
    assertEquals(16f.px, frames.titleFrame.left)
    assertEquals(16f.px, frames.titleFrame.top)
    assertEquals(16f.px, frames.subtitleFrame.left)
    assertEquals(16f.px + title.height, frames.subtitleFrame.top)

    val bare = layout(ShadowListUtilsSnapCard(ShadowListUtilsSnapItem("bare"), 240f, theme, labels))
    assertEquals(Rect(), bare.captionFrame)
    assertNull(bare.title)
    assertNull(bare.subtitle)
  }

  @Test
  fun a_right_to_left_card_mirrors_its_frames() {
    val card = ShadowListUtilsSnapCard(item(0), 240f, theme, labels)
    val plain = layout(card)
    val mirrored = withRightToLeft { layout(card) }
    assertEquals(plain.cardFrame, mirrored.cardFrame)
    assertEquals(plain.captionFrame, mirrored.captionFrame)
    // The text frames mirror inside the card, which is narrower than the row.
    assertEquals(plain.cardFrame.width() - 16f.px, mirrored.titleFrame.right)
    assertEquals(plain.cardFrame.width() - plain.subtitleFrame.right, mirrored.subtitleFrame.left)
  }

  @Test
  fun the_card_reads_its_text_its_alt_or_the_label() = onMain {
    val view = ShadowListUtilsSnapCardView(testContext)
    view.configure(ShadowListUtilsSnapCard(item(0), 240f, theme, labels), null)
    assertEquals("Lisbon, Portugal", view.contentDescription)
    // An empty string is left out.
    view.configure(ShadowListUtilsSnapCard(item(0).copy(title = ""), 240f, theme, labels), null)
    assertEquals("Portugal", view.contentDescription)
    view.configure(ShadowListUtilsSnapCard(ShadowListUtilsSnapItem("alt", imageAlt = "A tram"), 240f, theme, labels), null)
    assertEquals("A tram", view.contentDescription)
    view.configure(ShadowListUtilsSnapCard(ShadowListUtilsSnapItem("bare"), 240f, theme, labels), null)
    assertEquals("Card", view.contentDescription)
  }
}
