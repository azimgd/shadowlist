package com.shadowlist.kit

import android.app.Activity
import android.content.Intent
import android.os.SystemClock
import android.view.MotionEvent
import android.view.ViewGroup
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/*
 * Test-only window: real hit testing and detachment, without adding a host to the shipped AAR.
 */
class ShadowListKitBoardTestActivity : Activity()

class ShadowListKitDragBoardTest {
  private class Rows(val keys: MutableList<String>) :
    ShadowListKitListView.DataSource,
    ShadowListKitListView.Sizing {
    override fun numberOfItems(listView: ShadowListKitListView): Int = keys.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = keys[index]
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
      listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = 100
  }

  private class Board(activity: Activity, empty: Boolean, nested: Boolean) {
    val root = FrameLayout(activity)
    val source = ShadowListKitListView(activity)
    val destination = ShadowListKitListView(activity)
    val sourceRows = Rows(mutableListOf("home0", "home1", "home2"))
    val destinationRows = Rows(if (empty) mutableListOf() else mutableListOf("target0", "target1"))
    var requests = 0
    var accept = false
    var detach = false
    var canMove = true
    var outer: ShadowListKitListView? = null

    init {
      for ((list, rows, key) in listOf(
        Triple(source, sourceRows, "source"),
        Triple(destination, destinationRows, "destination")
      )) {
        list.registerCell("row") { ShadowListKitListCell(it, "row") }
        list.dataSource = rows
        list.reorderEnabled = true
        list.dragBoardId = "window-test"
        list.dragListId = key
      }
      source.delegate = object : ShadowListKitListView.Delegate {
        override fun canMoveItem(listView: ShadowListKitListView, index: Int): Boolean = canMove
        override fun moveItemToList(
          listView: ShadowListKitListView,
          sourceIndex: Int,
          destinationListId: String,
          destinationIndex: Int,
          destinationKey: String
        ) {
          ++requests
          assertEquals("destination", destinationListId)
          if (detach) {
            root.removeAllViews()
          } else if (accept) {
            val key = sourceRows.keys.removeAt(sourceIndex)
            destinationRows.keys.add(destinationIndex, key)
          }
        }
      }
      if (nested) {
        val strip = ShadowListKitListView(activity)
        outer = strip
        strip.horizontal = true
        strip.snapToItem = true
        strip.overscan = 4.0
        strip.mountOverscan = 4.0
        val columns = listOf(source, destination)
        for ((index, list) in columns.withIndex()) {
          strip.registerCell("column$index") {
            ShadowListKitListCell(it, "column$index").apply {
              addView(
                list,
                FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
              )
            }
          }
        }
        strip.dataSource = object : ShadowListKitListView.DataSource, ShadowListKitListView.Sizing {
          override fun numberOfItems(listView: ShadowListKitListView): Int = columns.size
          override fun keyForItem(listView: ShadowListKitListView, index: Int): String = "column$index"
          override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
            listView.dequeueReusableCell("column$index")
          override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = (
            activity.resources.displayMetrics.widthPixels *
              0.6
            ).toInt()
        }
        root.addView(
          strip,
          FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
        )
        strip.reloadData()
      } else {
        root.addView(
          source,
          FrameLayout.LayoutParams(
            activity.resources.displayMetrics.widthPixels / 2,
            ViewGroup.LayoutParams.MATCH_PARENT
          )
        )
        root.addView(
          destination,
          FrameLayout.LayoutParams(
            activity.resources.displayMetrics.widthPixels / 2,
            ViewGroup.LayoutParams.MATCH_PARENT
          ).apply {
            leftMargin = activity.resources.displayMetrics.widthPixels / 2
          }
        )
      }
      activity.setContentView(root)
      source.reloadData()
      destination.reloadData()
    }

    fun hover() {
      source.layoutPass()
      destination.layoutPass()
      assertTrue(source.beginDragAt(source.width / 2f, 20f))
      val from = IntArray(2)
      val to = IntArray(2)
      source.getLocationOnScreen(from)
      destination.getLocationOnScreen(to)
      val event = MotionEvent.obtain(
        SystemClock.uptimeMillis(),
        SystemClock.uptimeMillis(),
        MotionEvent.ACTION_MOVE,
        to[0] - from[0] + destination.width / 2f,
        to[1] - from[1] + 20f,
        0
      )
      try {
        source.drag.handle(event)
      } finally {
        event.recycle()
      }
      assertTrue(destination.core.isDragForeign)
      assertEquals(-1, source.core.dragInsertionIndex)
      assertEquals(0f, source.heldCell!!.alpha)
    }

    fun assertEnded() {
      assertFalse(source.hasHeldRow)
      assertFalse(destination.core.isDragForeign)
      assertEquals(-1, destination.core.dragInsertionIndex)
    }
  }

  private fun withBoard(empty: Boolean = false, nested: Boolean = false, body: (Board) -> Unit) {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(
      Intent().setClassName(
        context.packageName,
        ShadowListKitBoardTestActivity::class.java.name
      ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    )
    var board: Board? = null
    try {
      onMain { board = Board(activity, empty, nested) }
      instrumentation.waitForIdleSync()
      onMain { body(board!!) }
    } finally {
      onMain { activity.finish() }
      instrumentation.waitForIdleSync()
    }
  }

  private fun onMain(body: () -> Unit) {
    var failure: Throwable? = null
    InstrumentationRegistry.getInstrumentation().runOnMainSync {
      try {
        body()
      } catch (thrown: Throwable) {
        failure = thrown
      }
    }
    failure?.let { throw it }
  }

  @Test fun an_accepted_cross_list_drop_ends_both_sessions() = withBoard { board ->
    board.accept = true
    board.hover()
    board.source.drag.end(true)
    assertEquals(1, board.requests)
    assertEquals(listOf("home1", "home2"), board.sourceRows.keys)
    assertEquals("home0", board.destinationRows.keys.first())
    board.assertEnded()
  }

  @Test fun a_rejected_drop_keeps_the_item_and_clears_foreign_shifts() = withBoard { board ->
    board.hover()
    board.source.drag.end(true)
    assertEquals(1, board.requests)
    assertEquals("home0", board.sourceRows.keys.first())
    assertFalse(board.destinationRows.keys.contains("home0"))
    board.assertEnded()
  }

  @Test fun cancellation_requests_no_move_and_clears_the_foreign_session() = withBoard { board ->
    board.hover()
    board.source.drag.end(false)
    assertEquals(0, board.requests)
    board.assertEnded()
  }

  @Test fun an_empty_destination_accepts_at_zero() = withBoard(empty = true) { board ->
    board.accept = true
    board.hover()
    board.source.drag.end(true)
    assertEquals(listOf("home0"), board.destinationRows.keys)
    board.assertEnded()
  }

  @Test fun callback_detaching_both_lists_never_revives_their_destroyed_peers() = withBoard { board ->
    board.detach = true
    board.hover()
    board.source.drag.end(true)
    assertEquals(1, board.requests)
    assertNull(board.source.coreOrNull)
    assertNull(board.destination.coreOrNull)
    assertNull(board.source.dragBoard)
    assertNull(board.destination.dragBoard)
  }

  @Test fun deleting_the_source_key_cancels_the_foreign_session() = withBoard { board ->
    board.hover()
    board.sourceRows.keys.removeAt(0)
    board.source.reloadData()
    board.source.layoutPass()
    assertEquals(0, board.requests)
    board.assertEnded()
  }

  @Test fun accessibility_appends_with_the_target_label_and_the_same_controlled_callback() = withBoard { board ->
    board.accept = true
    board.destination.dragListLabel = "Done"
    val cell = board.source.cellForItem(0)!!
    val info = AccessibilityNodeInfo.obtain()
    cell.onInitializeAccessibilityNodeInfo(info)
    val action = info.actionList.first { it.label == "Move to Done" }
    assertTrue(cell.performAccessibilityAction(action.id, null))
    assertEquals(1, board.requests)
    assertEquals(listOf("target0", "target1", "home0"), board.destinationRows.keys)
    board.assertEnded()
    info.recycle()
  }

  @Test fun accessibility_rechecks_movability_and_rejection_keeps_the_data() = withBoard { board ->
    val cell = board.source.cellForItem(0)!!
    val info = AccessibilityNodeInfo.obtain()
    cell.onInitializeAccessibilityNodeInfo(info)
    val action = info.actionList.first { it.label == "Move to destination" }
    board.canMove = false
    cell.performAccessibilityAction(action.id, null)
    assertEquals(0, board.requests)
    board.canMove = true
    assertTrue(cell.performAccessibilityAction(action.id, null))
    assertEquals(1, board.requests)
    assertEquals("home0", board.sourceRows.keys.first())
    assertFalse(board.destinationRows.keys.contains("home0"))
    board.assertEnded()
    info.recycle()
  }

  @Test fun accessibility_callback_detaching_both_lists_keeps_native_peers_destroyed() = withBoard { board ->
    board.detach = true
    val cell = board.source.cellForItem(0)!!
    val info = AccessibilityNodeInfo.obtain()
    cell.onInitializeAccessibilityNodeInfo(info)
    val action = info.actionList.first { it.label == "Move to destination" }
    assertTrue(cell.performAccessibilityAction(action.id, null))
    assertNull(board.source.coreOrNull)
    assertNull(board.destination.coreOrNull)
    info.recycle()
  }

  @Test fun a_horizontal_kit_host_scrolls_under_the_same_finger_without_detaching_columns() =
    withBoard(nested = true) { board ->
      board.hover()
      val outer = board.outer!!
      val peer = board.source.core.handle
      val cell = board.source.heldCell
      val time = System.nanoTime()
      // Drive the existing frame callback with a held finger near the board's trailing edge.
      for (frame in 1..20) board.source.drag.doFrame(time + frame * 16_666_667L)
      assertTrue(outer.offset > 0)
      assertEquals(peer, board.source.core.handle)
      assertTrue(board.source.isAttachedToWindow)
      assertTrue(board.destination.isAttachedToWindow)
      assertTrue(board.source.heldCell === cell)
      board.source.drag.end(false)
      board.assertEnded()
    }
}
