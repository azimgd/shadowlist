package com.shadowlist.kit

import android.content.Context
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import android.view.View.MeasureSpec
import android.widget.FrameLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs
import kotlin.math.roundToInt

/*
 * What a view that hosts the list relies on: exact item sizes, driven mode and the scroll
 * switch, offset adjustments, batches that keep the visible rows still and the layout pass hook.
 */
@RunWith(AndroidJUnit4::class)
class ShadowListKitHostTest {
  companion object {
    private const val WIDTH = 1080
    private const val HEIGHT = 2000

    // 21dp at a density of 2.625.
    private const val ROW_SIZE = 21 * 2.625
  }

  private val context: Context = InstrumentationRegistry.getInstrumentation().targetContext

  // region Fixtures

  private class Rows(count: Int, private val size: Double) :
    ShadowListKitListView.DataSource, ShadowListKitListView.Sizing {
    val keys = MutableList(count) { "row$it" }
    private var nextKey = count

    fun newKey(): String = "row${nextKey++}"

    override fun numberOfItems(listView: ShadowListKitListView): Int = keys.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = keys[index]
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
      listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = size.roundToInt()
    override fun exactSizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Double = size
  }

  private class CountingListView(context: Context) : ShadowListKitListView(context) {
    var layoutPasses = 0
    var mountedInPass = false

    override fun didLayOut() {
      layoutPasses += 1
      mountedInPass = visibleCells.isNotEmpty()
    }
  }

  /*
   * Counts the nested scrolls a child starts.
   */
  private class NestedParent(context: Context) : FrameLayout(context) {
    var nestedStarts = 0

    override fun onStartNestedScroll(child: View, target: View, axes: Int): Boolean {
      nestedStarts += 1
      return false
    }
  }

  private fun makeList(rows: Rows, list: ShadowListKitListView = ShadowListKitListView(context)): ShadowListKitListView {
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    list.dataSource = rows
    list.reloadData()
    layOut(list)
    return list
  }

  private fun layOut(view: View) {
    view.measure(
      MeasureSpec.makeMeasureSpec(WIDTH, MeasureSpec.EXACTLY),
      MeasureSpec.makeMeasureSpec(HEIGHT, MeasureSpec.EXACTLY))
    view.layout(0, 0, WIDTH, HEIGHT)
  }

  /*
   * Scroll by whole viewports until the offset reaches target. Every row on the way gets measured.
   */
  private fun scrollThrough(list: ShadowListKitListView, target: Int) {
    while (list.scrollY < target) {
      val before = list.scrollY
      list.scrollTo(0, minOf(target, before + HEIGHT * 3 / 4))
      if (list.scrollY == before) break
    }
  }

  /*
   * Where each visible row's cell is on screen, by key.
   */
  private fun screenTops(list: ShadowListKitListView, rows: Rows): Map<String, Int> =
    list.visibleCells.associate { rows.keys[it.index] to it.top - list.scrollY }

  /*
   * Every visible cell sits at the pixel rounded from its exact frame and the exact offset.
   */
  private fun assertPlacedAtExactOffset(list: ShadowListKitListView) {
    for (cell in list.visibleCells) {
      val top = list.rectForItem(cell.index)!!.top.toDouble()
      assertEquals((top - list.exactOffset).roundToInt(), cell.top - list.scrollY)
    }
  }

  /*
   * Views live on the main thread. A failed assertion there fails the test.
   */
  private fun onMain(block: () -> Unit) {
    var failure: Throwable? = null
    InstrumentationRegistry.getInstrumentation().runOnMainSync {
      try {
        block()
      } catch (thrown: Throwable) {
        failure = thrown
      }
    }
    failure?.let { throw it }
  }

  private fun touch(action: Int, y: Float, downTime: Long): MotionEvent =
    MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, WIDTH / 2f, y, 0)

  // endregion

  // region Exact sizes

  @Test
  fun ten_thousand_rows_of_fractional_size_end_where_their_exact_sizes_add_up() = onMain {
    val count = 10_000
    val rows = Rows(count, ROW_SIZE)
    val list = makeList(rows)
    scrollThrough(list, Int.MAX_VALUE)

    val exactEnd = count * ROW_SIZE
    assertEquals(exactEnd.roundToInt(), list.contentSize)
    assertEquals(list.contentSize - HEIGHT, list.scrollY)
    for (index in intArrayOf(0, 1, 7, 8, 999, 5_001, count - 1)) {
      val rect = list.rectForItem(index)!!
      assertEquals(index * ROW_SIZE, rect.top.toDouble(), 0.0)
      assertEquals(ROW_SIZE, rect.height().toDouble(), 1e-3)
    }

    // Cells sit on pixels rounded from exact running offsets, edge to edge.
    val cells = list.visibleCells
    assertTrue(cells.size > 30)
    for (cell in cells) assertTrue(abs(cell.top - cell.index * ROW_SIZE) <= 1.0)
    for (at in 1 until cells.size) assertEquals(cells[at - 1].bottom, cells[at].top)
    assertEquals(list.contentSize, cells.last().bottom)
  }

  @Test
  fun sizing_without_an_exact_size_keeps_whole_pixel_sizes() = onMain {
    val rows = object : ShadowListKitListView.DataSource, ShadowListKitListView.Sizing {
      override fun numberOfItems(listView: ShadowListKitListView): Int = 100
      override fun keyForItem(listView: ShadowListKitListView, index: Int): String = "row$index"
      override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
        listView.dequeueReusableCell("row")
      override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = 55
    }
    val list = ShadowListKitListView(context)
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    list.dataSource = rows
    list.reloadData()
    layOut(list)
    assertEquals(55f * 30, list.rectForItem(30)!!.top)
  }

  // endregion

  // region Driven and scroll switches

  /*
   * Takes the moves of a touch once it pans past the slop, the way a view that owns the
   * scrolling does, and counts them.
   */
  private class PanningParent(context: Context) : FrameLayout(context) {
    var moves = 0
    private var downY = 0f

    override fun onInterceptTouchEvent(event: MotionEvent): Boolean {
      if (event.actionMasked == MotionEvent.ACTION_DOWN) downY = event.y
      return event.actionMasked == MotionEvent.ACTION_MOVE && abs(event.y - downY) > 20
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
      if (event.actionMasked == MotionEvent.ACTION_MOVE) moves += 1
      return true
    }
  }

  private fun drag(parent: View, from: Float, to: FloatArray) {
    val downTime = SystemClock.uptimeMillis()
    parent.dispatchTouchEvent(touch(MotionEvent.ACTION_DOWN, from, downTime))
    for (y in to) parent.dispatchTouchEvent(touch(MotionEvent.ACTION_MOVE, y, downTime))
    parent.dispatchTouchEvent(touch(MotionEvent.ACTION_UP, to.last(), downTime))
  }

  @Test
  fun a_driven_list_takes_no_drag_and_a_tap_through_its_parent_still_selects() = onMain {
    val parent = NestedParent(context)
    val list = ShadowListKitListView(context)
    parent.addView(list)
    makeList(Rows(500, ROW_SIZE), list)
    layOut(parent)
    list.scrollDriven = true

    drag(parent, 1500f, floatArrayOf(1400f, 1200f, 900f, 500f))
    assertEquals(0, list.scrollY)
    assertEquals(0, parent.nestedStarts)

    drag(parent, 100f, floatArrayOf(100f))
    assertEquals(listOf((100 / ROW_SIZE).toInt()), list.selectedIndices.toList())

    // The host moves the list instead.
    list.scrollTo(0, 1000)
    assertEquals(1000, list.scrollY)
  }

  @Test
  fun a_parent_that_pans_takes_the_moves_of_a_driven_list() = onMain {
    val parent = PanningParent(context)
    val list = ShadowListKitListView(context)
    parent.addView(list)
    makeList(Rows(500, ROW_SIZE), list)
    layOut(parent)
    list.scrollDriven = true

    drag(parent, 1500f, floatArrayOf(1490f, 1400f, 1200f, 900f))
    assertTrue(parent.moves >= 2)
    assertEquals(0, list.scrollY)
    assertTrue(list.selectedIndices.isEmpty())
  }

  @Test
  fun a_list_without_scrolling_takes_no_drag_and_keeps_its_animated_commands() = onMain {
    val parent = NestedParent(context)
    val list = ShadowListKitListView(context)
    parent.addView(list)
    makeList(Rows(500, ROW_SIZE), list)
    layOut(parent)
    list.scrollEnabled = false

    drag(parent, 1500f, floatArrayOf(1400f, 1200f, 900f, 500f))
    assertEquals(0, list.scrollY)
    assertEquals(0, parent.nestedStarts)

    list.scrollToItem(300, 0.0, animated = true)
    assertTrue(list.scrollY < 300 * ROW_SIZE - 1)
  }

  @Test
  fun a_driven_list_lands_an_animated_command_at_once() = onMain {
    val list = makeList(Rows(500, ROW_SIZE))
    list.scrollDriven = true
    list.scrollToItem(300, 0.0, animated = true)
    assertEquals(list.rectForItem(300)!!.top.toDouble(), list.exactOffset, 1.0)
    assertEquals(300, list.visibleRange!!.first)
  }

  @Test
  fun a_list_with_scrolling_on_takes_the_same_drag() = onMain {
    val parent = NestedParent(context)
    val list = ShadowListKitListView(context)
    parent.addView(list)
    makeList(Rows(500, ROW_SIZE), list)
    layOut(parent)

    drag(parent, 1500f, floatArrayOf(1400f, 1200f, 900f, 500f))
    assertTrue(list.scrollY > 0)
    assertTrue(parent.nestedStarts > 0)
  }

  // endregion

  // region Offset and batches

  @Test
  fun a_batch_that_deletes_the_head_and_inserts_at_the_tail_keeps_the_visible_rows_still() = onMain {
    val rows = Rows(2_000, ROW_SIZE)
    val list = makeList(rows)
    scrollThrough(list, 60_000)
    val before = screenTops(list, rows)
    val offsetBefore = list.scrollY
    assertTrue(before.size > 30)

    // 333 rows of 55.125 pixels leave a fraction of a pixel.
    val moved = 333
    list.performBatchUpdates({
      repeat(moved) { rows.keys.removeAt(0) }
      repeat(moved) { rows.keys.add(rows.newKey()) }
      list.deleteItems(IntArray(moved) { it })
      list.insertItems(IntArray(moved) { rows.keys.size - moved + it })
    })
    layOut(list)

    assertEquals(before, screenTops(list, rows))
    assertTrue(abs(offsetBefore - list.scrollY - moved * ROW_SIZE) <= 1.0)
    assertPlacedAtExactOffset(list)
  }

  @Test
  fun adjusting_the_offset_moves_the_rows_by_the_delta_and_lays_out() = onMain {
    val rows = Rows(2_000, ROW_SIZE)
    val list = CountingListView(context)
    makeList(rows, list)
    scrollThrough(list, 20_000)
    val before = screenTops(list, rows)
    val offsetBefore = list.scrollY
    val passes = list.layoutPasses

    list.adjustOffsetBy(-37.0)

    assertEquals(offsetBefore - 37, list.scrollY)
    assertTrue(list.layoutPasses > passes)
    val after = screenTops(list, rows)
    for ((key, top) in before) after[key]?.let { assertEquals(top + 37, it) }

    // Fractions add up to whole pixels, and the cells follow each one.
    val exactBefore = list.exactOffset
    for (step in 1..4) {
      list.adjustOffsetBy(0.25)
      assertEquals(exactBefore + step * 0.25, list.exactOffset, 1e-9)
      assertPlacedAtExactOffset(list)
    }
    assertEquals(offsetBefore - 36, list.scrollY)
  }

  @Test
  fun an_offset_moved_before_the_start_comes_back_to_it() = onMain {
    val list = makeList(Rows(2_000, ROW_SIZE))
    list.adjustOffsetBy(-37.0)
    assertEquals(0, list.scrollY)
    assertEquals(0.0, list.exactOffset, 0.0)
    assertEquals(0, list.visibleCells.minOf { it.top })
  }

  // endregion

  // region Layout pass hook

  @Test
  fun the_layout_pass_hook_runs_after_every_pass_with_the_cells_mounted() = onMain {
    val rows = Rows(500, ROW_SIZE)
    val list = CountingListView(context)
    makeList(rows, list)
    assertTrue(list.layoutPasses > 0)
    assertTrue(list.mountedInPass)

    var passes = list.layoutPasses
    list.scrollTo(0, 3_000)
    assertTrue(list.layoutPasses > passes)

    passes = list.layoutPasses
    list.performBatchUpdates({
      rows.keys.add(0, rows.newKey())
      list.insertItems(intArrayOf(0))
    })
    layOut(list)
    assertTrue(list.layoutPasses > passes)
    assertTrue(list.mountedInPass)
  }

  // endregion

  // region Accessibility

  @Test
  fun a_list_hidden_from_accessibility_stays_hidden_with_its_cells() = onMain {
    val list = ShadowListKitListView(context)
    list.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
    val rows = Rows(500, ROW_SIZE)
    makeList(rows, list)
    list.scrollTo(0, 5_000)
    list.reloadData()
    layOut(list)

    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS, list.importantForAccessibility)
    assertFalse(list.isImportantForAccessibility)
    assertTrue(list.visibleCells.isNotEmpty())
    for (cell in list.visibleCells) assertFalse(cell.isImportantForAccessibility)
  }

  // endregion
}
