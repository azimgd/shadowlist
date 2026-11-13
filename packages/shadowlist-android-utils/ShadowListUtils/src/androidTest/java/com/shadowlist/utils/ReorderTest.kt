package com.shadowlist.utils

import android.graphics.Rect
import android.graphics.RectF
import android.os.Build
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.max

@RunWith(AndroidJUnit4::class)
class ReorderTest {
  private val theme = testTheme
  private val labels = ShadowListUtilsReorderLabels()
  private val width = TEST_WIDTH_DP.px

  // The width of one of three columns of a phone.
  private val tileWidth = 130f.px

  private fun tile(index: Int) = ShadowListUtilsReorderTileItem(key = "tile-$index", title = "Tile $index", label = "$index")

  private fun makeList(count: Int): ShadowListUtilsReorderList {
    val list = ShadowListUtilsReorderList(testContext, theme)
    list.items = Fixture.contacts(count)
    host(list)
    return list
  }

  private fun makeGrid(items: List<ShadowListUtilsReorderTileItem>): ShadowListUtilsReorderGrid {
    val grid = ShadowListUtilsReorderGrid(testContext, theme)
    grid.items = items
    host(grid)
    return grid
  }

  private fun layout(row: ShadowListUtilsReorderRow) = row.layout(width) as ShadowListUtilsReorderRowLayout

  private fun layout(tile: ShadowListUtilsReorderTile) = tile.layout(tileWidth) as ShadowListUtilsReorderTileLayout

  private fun ShadowListUtilsList.frame(index: Int): RectF = requireNotNull(rectForItem(index)) { "item $index has no frame" }

  /*
   * The labels of the two move actions a screen reader is offered for a row, backward first.
   */
  private fun moveActionLabels(view: View): List<String> {
    val actions = view.createAccessibilityNodeInfo().actionList
    return listOf(R.id.shadowlist_utils_action_move_backward, R.id.shadowlist_utils_action_move_forward).mapNotNull { id ->
      actions.firstOrNull { it.id == id }?.label?.toString()
    }
  }

  /*
   * A node has a hint from API 26. Before it the hint ends the label.
   */
  private fun assertHint(expected: String, view: View) {
    val node = view.createAccessibilityNodeInfo()
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
      assertEquals(expected, node.hintText?.toString())
    } else {
      assertTrue(node.contentDescription.toString().endsWith(expected))
    }
  }

  @Test
  fun the_list_shows_its_items() = onMain {
    val list = makeList(5)
    assertEquals(List(5) { "contact-$it" }, list.keys)
    assertTrue(list.rowView(0) is ShadowListUtilsReorderRowView)
  }

  @Test
  fun the_list_and_the_grid_reorder_and_the_grid_has_three_columns() = onMain {
    val list = ShadowListUtilsReorderList(testContext, theme)
    assertTrue(list.reorderEnabled)
    assertEquals(1, list.numberOfColumns)
    assertFalse(list.allowsSelection)
    val grid = ShadowListUtilsReorderGrid(testContext, theme)
    assertTrue(grid.reorderEnabled)
    assertEquals(3, grid.numberOfColumns)
    assertFalse(grid.allowsSelection)
  }

  @Test
  fun a_changed_item_is_configured_again_and_a_deleted_item_leaves() = onMain {
    val list = makeList(4)
    val items = list.items.toMutableList()
    val untouched = list.rowView(0)
    val untouchedRow = untouched?.row
    items[2] = items[2].copy(name = "Renamed")
    list.setItems(items, false)
    host(list)
    assertEquals("Renamed", (list.rowView(2)?.row as? ShadowListUtilsReorderRow)?.item?.name)
    assertSame(list.rows[2], list.rowView(2)?.row)
    assertSame(untouched, list.rowView(0))
    // An item that equals the one before keeps its row.
    assertSame(untouchedRow, list.rowView(0)?.row)
    items.removeAt(items.lastIndex)
    list.setItems(items, true)
    assertEquals(3, list.keys.size)
  }

  @Test
  fun a_drop_moves_the_item_and_runs_on_move_item_with_the_items_after_the_move() = onMain {
    val list = makeList(5)
    val moves = ArrayList<Triple<Int, Int, List<String>>>()
    list.onMoveItem = { sourceIndex, destinationIndex, items -> moves.add(Triple(sourceIndex, destinationIndex, items.map { it.key })) }
    list.drop(1, 3)
    val expected = listOf("contact-0", "contact-2", "contact-3", "contact-1", "contact-4")
    assertEquals(1, moves.size)
    assertEquals(1, moves[0].first)
    assertEquals(3, moves[0].second)
    assertEquals(expected, moves[0].third)
    assertEquals(expected, list.items.map { it.key })
    assertEquals(expected, list.keys)
    host(list)
    assertEquals(expected, List(5) { list.rowView(it)?.row?.key })
  }

  @Test
  fun a_drop_in_place_does_not_run_on_move_item() = onMain {
    val list = makeList(3)
    val grid = makeGrid(List(3, ::tile))
    var moves = 0
    list.onMoveItem = { _, _, _ -> ++moves }
    grid.onMoveItem = { _, _, _ -> ++moves }
    list.drop(1, 1)
    grid.drop(2, 2)
    assertEquals(0, moves)
    assertEquals(listOf("contact-0", "contact-1", "contact-2"), list.keys)
    assertEquals(listOf("tile-0", "tile-1", "tile-2"), grid.items.map { it.key })
    list.drop(1, 0)
    assertEquals(1, moves)
  }

  @Test
  fun a_screen_reader_moves_a_row_by_one_place() = onMain {
    val list = makeList(3)
    val view = requireNotNull(list.rowView(0))
    assertEquals(emptyList<String>(), moveActionLabels(view))
    assertFalse(view.performAccessibilityAction(R.id.shadowlist_utils_action_move_forward, null))
    assertEquals("Person 0, Seat 0", view.contentDescription)
    assertHint("Press and hold, then drag to reorder", view)
    var moved = emptyList<String>()
    var moves = 0
    list.onMoveItem = { _, _, items ->
      moved = items.map { it.key }
      ++moves
    }
    assertEquals(listOf("Move up", "Move down"), moveActionLabels(view))
    // The first row has no place before it.
    assertFalse(view.performAccessibilityAction(R.id.shadowlist_utils_action_move_backward, null))
    assertEquals(0, moves)
    assertTrue(view.performAccessibilityAction(R.id.shadowlist_utils_action_move_forward, null))
    assertEquals(listOf("contact-1", "contact-0", "contact-2"), moved)
    assertEquals(moved, list.keys)
    assertEquals(moved, list.items.map { it.key })
    assertEquals(1, moves)
  }

  @Test
  fun the_grid_moves_tiles_and_names_its_actions_earlier_and_later() = onMain {
    val grid = makeGrid(List(6, ::tile))
    assertTrue(grid.rowView(0) is ShadowListUtilsReorderTileView)
    val moves = ArrayList<List<String>>()
    grid.onMoveItem = { _, _, items ->
      assertEquals(grid.items, items)
      moves.add(items.map { it.key })
    }
    grid.drop(0, 5)
    assertEquals(1, moves.size)
    assertEquals(listOf("tile-1", "tile-2", "tile-3", "tile-4", "tile-5", "tile-0"), moves.last())
    assertEquals(moves.last(), grid.items.map { it.key })
    host(grid)
    val view = requireNotNull(grid.rowView(0))
    assertEquals(listOf("Move earlier", "Move later"), moveActionLabels(view))
    assertEquals("1, Tile 1", view.contentDescription)
    assertHint("Press and hold, then drag to reorder", view)
  }

  @Test
  fun the_row_layout_holds_its_metrics() {
    val frames = layout(ShadowListUtilsReorderRow(Fixture.contact(0), theme, labels))
    val body = theme.typography.body
    val subhead = theme.typography.subhead
    val textHeight = body.scaledLineHeightPx() + 1f.px + subhead.scaledLineHeightPx()
    val height = max(40f.px, textHeight) + 2 * 12f.px
    assertEquals(height, frames.heightPx)
    // 12 + 22 + 1 + 20 + 12 at a text size of 1, within the rounding of its parts.
    assertEquals(67f.px.toFloat(), frames.heightPx.toFloat(), 3f)
    val avatarTop = (height - 40f.px) / 2
    assertEquals(Rect(16f.px, avatarTop, 16f.px + 40f.px, avatarTop + 40f.px), frames.avatarFrame)
    val textLeft = 16f.px + 40f.px + 12f.px
    val textTop = (height - textHeight) / 2
    assertEquals(textLeft, frames.nameFrame.left)
    assertEquals(textTop, frames.nameFrame.top)
    assertEquals(textLeft, frames.subtitleFrame.left)
    assertEquals(textTop + body.scaledLineHeightPx() + 1f.px, frames.subtitleFrame.top)
    val gripTop = (height - 20f.px) / 2
    assertEquals(Rect(width - 16f.px - 20f.px, gripTop, width - 16f.px, gripTop + 20f.px), frames.gripFrame)
    assertEquals(68f.px, frames.separatorFrame.left)
    assertEquals(height, frames.separatorFrame.bottom)
    assertEquals(1, frames.name.lineCount)
    assertLines(frames.name, body, 1)
    assertLines(frames.subtitle, subhead, 1)
  }

  @Test
  fun the_tile_layout_holds_its_metrics() {
    val footnote = theme.typography.footnote
    val swatch = tileWidth - 2 * 6f.px
    val frames = layout(ShadowListUtilsReorderTile(tile(0), null, theme, labels))
    assertEquals(Rect(6f.px, 0, 6f.px + swatch, swatch), frames.swatchFrame)
    assertEquals(frames.swatchFrame.exactCenterX(), frames.labelFrame.exactCenterX(), 1f)
    assertEquals(frames.swatchFrame.exactCenterY(), frames.labelFrame.exactCenterY(), 1f)
    assertEquals(6f.px + 4f.px, frames.titleFrame.left)
    assertEquals(swatch + 4f.px, frames.titleFrame.top)
    assertLines(frames.title, footnote, 1)
    assertEquals(swatch + 4f.px + footnote.scaledLineHeightPx() + 12f.px, frames.heightPx)

    val long = tile(1).copy(title = "word ".repeat(20), aspectRatio = 2f)
    val wrapped = layout(ShadowListUtilsReorderTile(long, null, theme, labels))
    // Half the swatch's width, on the pixel grid.
    assertEquals(swatch / 2f, wrapped.swatchFrame.height().toFloat(), 0.5f)
    assertEquals(2, wrapped.title.lineCount)
    assertLines(wrapped.title, footnote, 2)
    assertEquals(wrapped.titleFrame.bottom + 12f.px, wrapped.heightPx)
    val even = layout(ShadowListUtilsReorderTile(long, 1f, theme, labels))
    assertEquals(swatch, even.swatchFrame.height())
    assertEquals(1, even.title.lineCount)

    val unlabelled = layout(ShadowListUtilsReorderTile(tile(2).copy(label = null), null, theme, labels))
    assertNull(unlabelled.label)
    assertTrue(unlabelled.labelFrame.isEmpty)
  }

  @Test
  fun tile_aspect_ratio_gives_every_tile_of_the_grid_one_height() = onMain {
    val items = List(6, ::tile).toMutableList()
    items[1] = items[1].copy(aspectRatio = 0.5f)
    val grid = makeGrid(items)
    assertNotEquals(grid.frame(0).height(), grid.frame(1).height())
    grid.tileAspectRatio = 1f
    host(grid)
    assertEquals(grid.frame(0).height(), grid.frame(1).height(), 0f)
    assertEquals(grid.rowView(0)?.height, grid.rowView(1)?.height)
  }

  @Test
  fun a_tile_label_keeps_its_size_and_a_title_takes_four_lines_at_a_large_text_size() {
    val long = tile(0).copy(title = "word ".repeat(30))
    val own = ShadowListUtilsReorderTile(long, null, theme, labels)
    val even = ShadowListUtilsReorderTile(long, 1f, theme, labels)
    val plain = layout(own)
    val label = requireNotNull(plain.label)
    assertEquals(theme.typography.title2.scaledLineHeightPx(scaled = false), label.height)
    withTextSize(1.5f) {
      val large = layout(own)
      assertEquals(label.width, large.label?.width)
      assertEquals(label.height, large.label?.height)
      assertEquals(4, large.title.lineCount)
      assertLines(large.title, theme.typography.footnote, 4)
      assertEquals(1, layout(even).title.lineCount)
    }
    assertEquals(2, plain.title.lineCount)
  }

  @Test
  fun a_right_to_left_row_mirrors_its_frames() {
    val row = ShadowListUtilsReorderRow(Fixture.contact(0), theme, labels)
    val plain = layout(row)
    val mirrored = withRightToLeft { layout(row) }
    assertEquals(width - plain.avatarFrame.right, mirrored.avatarFrame.left)
    assertEquals(width - (16f.px + 40f.px + 12f.px), mirrored.nameFrame.right)
    assertEquals(width - plain.nameFrame.right, mirrored.nameFrame.left)
    assertEquals(16f.px, mirrored.gripFrame.left)
    val tile = ShadowListUtilsReorderTile(tile(0), null, theme, labels)
    val plainTile = layout(tile)
    val mirroredTile = withRightToLeft { layout(tile) }
    assertEquals(tileWidth - plainTile.titleFrame.left, mirroredTile.titleFrame.right)
  }
}
