package com.shadowlist.utils

import android.view.View
import com.shadowlist.kit.ShadowListKitBoardMovePlan
import com.shadowlist.kit.ShadowListKitListView
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BoardTest {
  private val columns = listOf(ShadowListUtilsBoardColumn("todo", "To do", listOf("a", "b")), ShadowListUtilsBoardColumn<String>("done", "Done", emptyList()))
  private fun makeBoard(): ShadowListUtilsBoardList<String> = ShadowListUtilsBoardList(testContext, keyExtractor = { item: String -> item },
    rowForItem = { ShadowListUtilsBoardCard(it, it, theme = testTheme) }, theme = testTheme).apply {
      columnWidth = 220f
      sidePadding = 12f
      this.columns = this@BoardTest.columns
    }

  @Test fun shared_board_move_vectors_pass_through_the_native_bridge() {
    for (entry in Vectors.cases("board-move.json")) {
      val input = entry.getJSONObject("input")
      val output = entry.getJSONObject("output")
      val raw = input.getJSONArray("columns")
      val keys = (0 until raw.length()).map { column -> raw.getJSONArray(column).let { values -> (0 until values.length()).map { values.getString(it) } } }
      val item = input.getString("itemKey")
      val destination = input.getInt("destinationColumn")
      val destinationKey = input.getString("destinationKey")
      val index = input.getInt("destinationIndex")
      val plan = ShadowListKitBoardMovePlan.plan(keys, item, destination, destinationKey, index)
      if (output.isNull("plan")) assertTrue(plan.isEmpty) else {
        val expected = output.getJSONObject("plan")
        assertEquals(expected.getInt("sourceColumn"), plan.sourceColumn)
        assertEquals(expected.getInt("sourceIndex"), plan.sourceIndex)
        assertEquals(expected.getInt("destinationColumn"), plan.destinationColumn)
        assertEquals(expected.getInt("destinationIndex"), plan.destinationIndex)
      }
      val columns = keys.mapIndexed { column, items -> ShadowListUtilsBoardColumn(column.toString(), column.toString(), items) }
      val move = ShadowListUtilsBoardMoveInfo(item, "source", destination.toString(), destinationKey, index)
      val moved = shadowListUtilsMoveBoardItem(columns, move) { it }
      val expected = output.getJSONArray("columns")
      assertEquals((0 until expected.length()).map { column -> expected.getJSONArray(column).let { values -> (0 until values.length()).map { values.getString(it) } } }, moved.map { it.items })
    }
  }

  @Test fun moves_are_controlled_and_the_application_can_accept_or_reject() = onMain {
    val board = makeBoard()
    var requested: ShadowListUtilsBoardMoveInfo<String>? = null
    board.onMoveItem = { requested = it }
    host(board, 600f.px, 600f.px)
    val source = requireNotNull(board.listForColumn("todo"))
    val target = requireNotNull(board.listForColumn("done"))
    source.delegate!!.moveItemToList(source, 0, target.dragListId!!, 0, "")
    assertEquals("done", requested?.destinationColumnKey)
    assertEquals(listOf(listOf("a", "b"), emptyList<String>()), board.columns.map { it.items })
    board.onMoveItem = { board.columns = shadowListUtilsMoveBoardItem(board.columns, it) { item -> item } }
    source.delegate!!.moveItemToList(source, 0, target.dragListId!!, 0, "")
    assertEquals(listOf(listOf("b"), listOf("a")), board.columns.map { it.items })
    assertEquals(listOf("b"), source.rows.map { it.key })
    assertEquals(listOf("a"), target.rows.map { it.key })
  }

  @Test fun column_views_preserve_identity_and_stay_isolated_between_boards() = onMain {
    val first = makeBoard()
    val second = makeBoard()
    val member = requireNotNull(first.listForColumn("todo"))
    first.columns = columns
    assertSame(member, first.listForColumn("todo"))
    assertNotEquals(member.dragBoardId, second.listForColumn("todo")?.dragBoardId)
    assertFalse(member.reorderEnabled)
    first.onMoveItem = {}
    assertTrue(member.reorderEnabled)
    first.columns = emptyList()
    assertNull(first.listForColumn("todo"))
  }

  @Test fun custom_nil_header_and_footer_stay_outside_the_draggable_rows() = onMain {
    val board = makeBoard()
    board.renderColumnHeader = { null }
    board.renderColumnFooter = { View(testContext).apply { minimumHeight = 44f.px } }
    host(board, 600f.px, 600f.px)
    val list = requireNotNull(board.listForColumn("todo"))
    assertEquals(0, (list.parent as View).top)
    assertEquals(2, list.rows.size)
    assertTrue(list.height < board.height)
  }

  @Test fun outer_kit_snaps_to_columns_without_detaching_their_lists() = onMain {
    val board = makeBoard()
    board.columns = (0 until 5).map { ShadowListUtilsBoardColumn("c$it", "Column $it", listOf("$it")) }
    host(board, 300f.px, 600f.px)
    val source = requireNotNull(board.listForColumn("c0"))
    val outer = board.getChildAt(0) as ShadowListKitListView
    assertTrue(outer.snapToItem)
    assertTrue(outer.horizontal)
    outer.scrollToItem(2, 0.0, false)
    host(board, 300f.px, 600f.px)
    assertEquals(requireNotNull(outer.rectForItem(2)).left.toDouble(), outer.scrollX.toDouble(), 1.0)
    assertNotNull(outer.cellForItem(0))
    assertNotNull(outer.cellForItem(4))
    assertSame(source, board.listForColumn("c0"))
    assertNotNull(source.parent)
  }

  @Test fun content_height_and_card_fields_use_the_shared_layout_contract() = onMain {
    val board = makeBoard()
    board.columnHeight = ShadowListUtilsBoardColumnHeight.CONTENT
    board.minColumnHeight = 120f
    board.maxColumnHeight = 180f
    host(board, 600f.px, 600f.px)
    val list = requireNotNull(board.listForColumn("todo"))
    assertTrue(list.height in 120f.px..180f.px)
    val row = ShadowListUtilsBoardCard("card", "Title", "Label", "Detail", 0xff123456.toInt(), theme = testTheme)
    val view = ShadowListUtilsBoardCardView(testContext)
    view.configure(row, row.layout(220f.px))
    assertEquals("Title, Label, Detail", view.contentDescription)
  }
}
