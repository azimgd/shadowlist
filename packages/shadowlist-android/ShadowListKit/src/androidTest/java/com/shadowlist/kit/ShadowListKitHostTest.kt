package com.shadowlist.kit

import android.content.Context
import android.graphics.RenderNode
import android.os.SystemClock
import android.text.TextPaint
import android.view.MotionEvent
import android.view.View
import android.view.View.MeasureSpec
import android.widget.FrameLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.roundToInt

/*
 * What a view that hosts the list relies on: exact item sizes, driven mode and the scroll
 * switch, offset adjustments, batches that keep the visible rows still and the layout pass hook.
 * Then what a list built on it relies on: the pinned header and footer, the empty view, the items
 * that never hold the content, whether an inverted list is at its end, long presses, the members
 * a subclass overrides and the lines of a text layout.
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

  /*
   * Rows whose sizes a test changes one by one.
   */
  private class SizedRows(count: Int, size: Double) :
    ShadowListKitListView.DataSource, ShadowListKitListView.Sizing {
    val keys = MutableList(count) { "row$it" }
    val sizes = MutableList(count) { size }

    override fun numberOfItems(listView: ShadowListKitListView): Int = keys.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = keys[index]
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
      listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = sizes[index].roundToInt()
    override fun exactSizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Double = sizes[index]
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
   * Tells the order its children draw in.
   */
  private class OrderedListView(context: Context) : ShadowListKitListView(context) {
    fun drawnChildren(): List<View> = List(childCount) { getChildAt(getChildDrawingOrder(childCount, it)) }
  }

  /*
   * A list that is its own data source and delegate once locked, and follows editing.
   */
  private class GuardedListView(context: Context) : ShadowListKitListView(context) {
    val editingCalls = ArrayList<Boolean>()
    var refused = 0
    var locked = false

    override var dataSource: DataSource?
      get() = super.dataSource
      set(value) {
        if (locked) refused += 1 else super.dataSource = value
      }

    override var delegate: Delegate?
      get() = super.delegate
      set(value) {
        if (locked) refused += 1 else super.delegate = value
      }

    override fun setEditing(editing: Boolean, animated: Boolean) {
      super.setEditing(editing, animated)
      editingCalls.add(editing)
    }
  }

  /*
   * Records what the list says about its end.
   */
  private class EndRecorder : ShadowListKitListView.Delegate {
    val changes = ArrayList<Boolean>()

    override fun didChangeAtEnd(listView: ShadowListKitListView, atEnd: Boolean) {
      changes.add(atEnd)
    }
  }

  private fun template(size: Int): View = View(context).apply { minimumHeight = size }

  private fun makeSizedList(rows: SizedRows, list: ShadowListKitListView = ShadowListKitListView(context)): ShadowListKitListView {
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    list.dataSource = rows
    list.reloadData()
    layOut(list)
    return list
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

  private fun layOut(view: View, height: Int = HEIGHT) {
    view.measure(
      MeasureSpec.makeMeasureSpec(WIDTH, MeasureSpec.EXACTLY),
      MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY))
    view.layout(0, 0, WIDTH, height)
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
   * Every row in view has its cell mounted and shown.
   */
  private fun assertVisibleRowsMounted(list: ShadowListKitListView) {
    for (index in list.visibleRange!!) assertTrue(list.cellForItem(index) != null)
  }

  /*
   * Draws the view the way a frame records it.
   */
  private fun draw(view: View) {
    val node = RenderNode("ShadowListKitHostTest")
    val canvas = node.beginRecording(WIDTH, HEIGHT)
    try {
      view.draw(canvas)
    } finally {
      node.endRecording()
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
  fun a_driven_list_moved_inside_its_mounted_rows_runs_neither_the_core_nor_a_mount_pass() = onMain {
    val rows = Rows(2_000, ROW_SIZE)
    val list = makeList(rows)
    list.scrollDriven = true
    scrollThrough(list, 20_000)
    // The pass after the jump publishes the band.
    list.adjustOffsetBy(1.0)
    val cores = list.corePassCount
    val mounts = list.mountPassCount
    val passes = list.layoutPassCount
    for (step in 1..3) {
      list.adjustOffsetBy(1.0)
      assertPlacedAtExactOffset(list)
    }
    assertEquals(cores, list.corePassCount)
    assertEquals(mounts, list.mountPassCount)
    assertEquals(passes + 3, list.layoutPassCount)

    // A move past the rows mounted ahead runs the core once and mounts them where they belong.
    val before = list.visibleRange!!
    list.adjustOffsetBy(HEIGHT.toDouble())
    assertEquals(cores + 1, list.corePassCount)
    assertTrue(list.mountPassCount > mounts)
    assertTrue(list.visibleRange!!.last > before.last)
    assertPlacedAtExactOffset(list)
    val bottom = list.visibleCells.maxOf { it.bottom } - list.scrollY
    assertTrue(bottom >= HEIGHT)
  }

  @Test
  fun a_driven_list_mounts_ahead_where_it_moves_and_plans_again_once_its_rows_stop_covering_the_viewport() = onMain {
    val rows = Rows(2_000, ROW_SIZE)
    val list = makeList(rows)
    list.scrollDriven = true
    scrollThrough(list, 20_000)
    // A short move past the rows mounted mounts a fifth of a viewport (400 pixels) ahead.
    list.adjustOffsetBy(100.0)
    val mounts = list.mountPassCount
    repeat(3) {
      list.adjustOffsetBy(100.0)
      assertVisibleRowsMounted(list)
      assertPlacedAtExactOffset(list)
    }
    assertEquals(mounts, list.mountPassCount)
    list.adjustOffsetBy(200.0)
    assertEquals(mounts + 1, list.mountPassCount)
    assertVisibleRowsMounted(list)
    assertPlacedAtExactOffset(list)

    // Back the other way it mounts ahead above: about one pass every four moves.
    val back = list.mountPassCount
    repeat(20) {
      list.adjustOffsetBy(-100.0)
      assertVisibleRowsMounted(list)
      assertPlacedAtExactOffset(list)
    }
    assertTrue(list.mountPassCount - back in 2..8)
  }

  @Test
  fun a_list_that_is_not_driven_mounts_the_rows_in_view_each_pass() = onMain {
    val list = makeList(Rows(2_000, ROW_SIZE))
    scrollThrough(list, 20_000)
    list.adjustOffsetBy(1.0)
    val mounts = list.mountPassCount
    list.adjustOffsetBy(ROW_SIZE * 2)
    assertEquals(mounts + 1, list.mountPassCount)
  }

  @Test
  fun a_driven_list_records_its_content_again_only_when_its_cells_change() = onMain {
    val rows = Rows(2_000, ROW_SIZE)
    val list = makeList(rows)
    list.scrollDriven = true
    scrollThrough(list, 20_000)
    list.adjustOffsetBy(100.0)
    draw(list)
    val records = list.contentRecordCount
    assertTrue(records > 0)

    // Moves inside the rows mounted ahead record only the list around its content.
    repeat(3) {
      list.adjustOffsetBy(100.0)
      draw(list)
    }
    assertEquals(records, list.contentRecordCount)

    // A cell that draws again does not record the content.
    list.visibleCells.first().invalidate()
    draw(list)
    assertEquals(records, list.contentRecordCount)

    // New rows shown record it once.
    list.adjustOffsetBy(HEIGHT.toDouble())
    draw(list)
    draw(list)
    assertEquals(records + 1, list.contentRecordCount)
  }

  @Test
  fun a_list_that_is_not_driven_draws_its_cells_itself() = onMain {
    val list = makeList(Rows(2_000, ROW_SIZE))
    scrollThrough(list, 5_000)
    draw(list)
    assertEquals(0, list.contentRecordCount)
  }

  @Test
  fun a_list_that_is_not_driven_runs_the_core_on_every_offset_adjustment() = onMain {
    val list = makeList(Rows(2_000, ROW_SIZE))
    scrollThrough(list, 20_000)
    list.adjustOffsetBy(1.0)
    val cores = list.corePassCount
    list.adjustOffsetBy(1.0)
    assertEquals(cores + 1, list.corePassCount)
  }

  @Test
  fun an_offset_moved_before_the_start_comes_back_to_it() = onMain {
    val list = makeList(Rows(2_000, ROW_SIZE))
    list.adjustOffsetBy(-37.0)
    assertEquals(0, list.scrollY)
    assertEquals(0.0, list.exactOffset, 0.0)
    assertEquals(0, list.visibleCells.minOf { it.top })
  }

  @Test
  fun the_footer_meets_the_last_row_at_a_fractional_offset() = onMain {
    // 41 rows end at 2260.125 pixels.
    val rows = Rows(41, ROW_SIZE)
    val list = ShadowListKitListView(context)
    list.footerView = View(context).apply { minimumHeight = 100 }
    makeList(rows, list)
    scrollThrough(list, Int.MAX_VALUE)
    list.adjustOffsetBy(-0.4)
    layOut(list)
    val last = list.visibleCells.maxByOrNull { it.index }!!
    assertEquals(rows.keys.size - 1, last.index)
    assertEquals(last.bottom, list.footerView!!.top)
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

  @Test
  fun the_last_row_grown_by_a_reload_takes_its_new_size() = onMain {
    val rows = SizedRows(3, ROW_SIZE)
    val list = ShadowListKitListView(context)
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    list.dataSource = rows
    list.reloadData()
    layOut(list)
    rows.sizes[2] = ROW_SIZE * 4
    list.reloadItems(intArrayOf(2))
    layOut(list)
    val rect = requireNotNull(list.rectForItem(2))
    val cell = requireNotNull(list.cellForItem(2))
    assertEquals(ROW_SIZE * 4, rect.height().toDouble(), 1.0)
    assertEquals(rect.height().toDouble(), cell.height.toDouble(), 1.0)
  }

  /*
   * The measured range first comes from the estimate, which ends far short of the viewport. The
   * pass fills the viewport before the frame is drawn.
   */
  @Test
  fun the_first_frame_fills_the_viewport_when_rows_are_shorter_than_their_estimate() = onMain {
    val list = ShadowListKitListView(context)
    list.estimatedItemSizePx = 10_000f
    makeList(Rows(3_000, 24.0), list)
    assertVisibleRowsMounted(list)
    assertTrue(list.visibleCells.maxOf { it.bottom } >= HEIGHT)
  }

  /*
   * Twelve items in four columns. The first and the last are tall and overlap every viewport,
   * which keeps the ends of the mount plan while the rows between them come and go.
   */
  private fun makeGrid(height: Int = HEIGHT): ShadowListKitListView {
    val rows = SizedRows(12, 400.0)
    rows.sizes[0] = 8000.0
    rows.sizes[2] = 1200.0
    rows.sizes[5] = 7600.0
    rows.sizes[6] = 6800.0
    rows.sizes[11] = 7600.0
    val list = ShadowListKitListView(context)
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    list.numberOfColumns = 4
    list.dataSource = rows
    list.reloadData()
    layOut(list, height)
    return list
  }

  @Test
  fun a_grid_mounts_a_row_that_comes_into_view_between_the_same_ends() = onMain {
    val list = makeGrid()
    assertEquals(8800, list.contentSize)
    assertTrue(list.cellForItem(2) != null)
    assertTrue(list.cellForItem(9) == null)
    list.scrollTo(0, 6800)
    for (index in intArrayOf(4, 9, 10)) {
      assertEquals(8000 - 6800, requireNotNull(list.cellForItem(index)).top - list.scrollY)
    }
    assertTrue(list.cellForItem(2) == null)
    list.scrollTo(0, 400)
    assertEquals(-400, requireNotNull(list.cellForItem(2)).top - list.scrollY)
    assertTrue(list.cellForItem(9) == null)
  }

  @Test
  fun a_grid_mounts_again_when_its_window_grows_over_more_rows() = onMain {
    val list = makeGrid(800)
    list.scrollTo(0, 5200)
    assertTrue(list.cellForItem(9) == null)
    layOut(list, 3200)
    assertEquals(8000 - 5200, requireNotNull(list.cellForItem(9)).top - list.scrollY)
  }

  /*
   * The core leaves an inverted list past its new end. The driver brings it back and runs the core
   * again there, and the end the first run reached is still told to the delegate.
   */
  @Test
  fun an_inverted_list_that_shrinks_to_a_few_rows_reaches_its_end() = onMain {
    var ends = 0
    val rows = Rows(400, ROW_SIZE)
    val list = ShadowListKitListView(context)
    list.inverted = true
    makeList(rows, list)
    list.scrollTo(0, 12_000)
    list.delegate = object : ShadowListKitListView.Delegate {
      override fun didReachEnd(listView: ShadowListKitListView) {
        ends += 1
      }
    }
    rows.keys.subList(3, rows.keys.size).clear()
    list.reloadData()
    layOut(list)
    assertEquals(1, ends)
    assertEquals(0, list.scrollY)
    layOut(list)
    assertEquals(1, ends)
  }

  /*
   * The rows the run at the new end measures are shorter than their estimates, and the content
   * shrinks once more.
   */
  @Test
  fun an_inverted_list_that_shrinks_twice_in_one_pass_rests_at_its_end() = onMain {
    val rows = SizedRows(400, 0.0)
    for (index in rows.sizes.indices) rows.sizes[index] = 120.0 + (index % 5) * 48
    val list = ShadowListKitListView(context)
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    list.inverted = true
    list.estimatedItemSizePx = 800f
    list.dataSource = rows
    list.reloadData()
    layOut(list, 1920)
    list.scrollTo(0, 12_000)
    rows.keys.subList(13, rows.keys.size).clear()
    list.deleteItems(IntArray(387) { 13 + it })
    layOut(list, 1920)
    assertEquals(list.contentSize - 1920, list.scrollY)
    layOut(list, 1920)
    assertEquals(2664, list.contentSize)
    assertEquals(744, list.scrollY)
  }

  @Test
  fun a_recycled_cell_comes_back_without_a_lift() = onMain {
    val rows = Rows(500, ROW_SIZE)
    val list = makeList(rows)
    val lifted = requireNotNull(list.cellForItem(2))
    // A drop cancelled part way leaves the lift on the cell.
    lifted.scaleX = 1.03f
    lifted.scaleY = 1.03f
    lifted.translationZ = 21f
    list.scrollTo(0, (300 * ROW_SIZE).roundToInt())
    // Row 2 left: its cell went back to the pool, or already shows another row.
    assertTrue(list.cellForItem(2) == null)
    assertEquals(1f, lifted.scaleX)
    assertEquals(1f, lifted.scaleY)
    assertEquals(0f, lifted.translationZ)
  }

  @Test
  fun sticky_rows_set_and_cleared_at_rest_pin_and_unpin() = onMain {
    val rows = Rows(500, ROW_SIZE)
    val list = makeList(rows)
    list.scrollTo(0, (8 * ROW_SIZE).roundToInt())
    list.stickyIndices = intArrayOf(5)
    layOut(list)
    val pinned = requireNotNull(list.cellForItem(5))
    assertEquals(list.scrollY, pinned.top)
    list.stickyIndices = IntArray(0)
    layOut(list)
    // Row 5 is above the viewport now: its cell is gone, or back at its own row.
    val back = list.cellForItem(5)
    if (back != null) assertEquals(requireNotNull(list.rectForItem(5)).top.roundToInt(), back.top)
    for (cell in list.visibleCells) {
      assertEquals(requireNotNull(list.rectForItem(cell.index)).top.roundToInt(), cell.top)
    }
  }

  /*
   * Rows with a content version a test bumps by key.
   */
  private class VersionedRows(count: Int) :
    ShadowListKitListView.DataSource, ShadowListKitListView.Sizing, ShadowListKitListView.ContentVersions {
    val keys = MutableList(count) { "row$it" }
    val versions = HashMap<String, Long>()

    override fun numberOfItems(listView: ShadowListKitListView): Int = keys.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = keys[index]
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
      listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = ROW_SIZE.roundToInt()
    override fun contentVersionForItem(listView: ShadowListKitListView, index: Int): Long = versions[keys[index]] ?: 0L
  }

  private fun makeVersionedList(rows: VersionedRows): ShadowListKitListView {
    val list = ShadowListKitListView(context)
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    list.dataSource = rows
    list.reloadData()
    layOut(list)
    return list
  }

  @Test
  fun an_item_inserted_through_its_own_call_is_reloaded_once_its_content_changes() = onMain {
    val rows = VersionedRows(10)
    val list = makeVersionedList(rows)
    rows.keys.add(2, "new")
    list.insertItems(intArrayOf(2))
    layOut(list)
    rows.versions["new"] = 1
    assertArrayEquals(intArrayOf(2), list.applyChanges().reloadedIndices)
    assertEquals(0, list.applyChanges().reloadedIndices.size)
  }

  @Test
  fun an_insert_keeps_the_change_of_another_item_for_the_next_diff() = onMain {
    val rows = VersionedRows(10)
    val list = makeVersionedList(rows)
    // Row 5 changed and nobody told the list yet.
    rows.versions["row5"] = 1
    rows.keys.add(0, "new")
    list.insertItems(intArrayOf(0))
    layOut(list)
    assertArrayEquals(intArrayOf(6), list.applyChanges().reloadedIndices)
  }

  @Test
  fun an_item_inserted_in_a_batch_is_reloaded_once_its_content_changes() = onMain {
    val rows = VersionedRows(10)
    val list = makeVersionedList(rows)
    rows.versions["row7"] = 1
    list.performBatchUpdates({
      rows.keys.removeAt(0)
      list.deleteItems(intArrayOf(0))
      rows.keys.add(3, "new")
      list.insertItems(intArrayOf(3))
    })
    layOut(list)
    rows.versions["new"] = 1
    assertArrayEquals(intArrayOf(3, 7), list.applyChanges().reloadedIndices)
  }

  /*
   * The pass reaches the end, then its measure throws. The edge is told once the list lays out.
   */
  @Test
  fun an_edge_reached_by_a_pass_whose_measure_threw_is_told_by_the_next_pass() = onMain {
    var throws = true
    var ends = 0
    val rows = object : ShadowListKitListView.DataSource, ShadowListKitListView.Sizing {
      override fun numberOfItems(listView: ShadowListKitListView): Int = 3
      override fun keyForItem(listView: ShadowListKitListView, index: Int): String = "row$index"
      override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
        listView.dequeueReusableCell("row")
      override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int {
        check(!throws) { "no size yet" }
        return 55
      }
    }
    val list = ShadowListKitListView(context)
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    list.delegate = object : ShadowListKitListView.Delegate {
      override fun didReachEnd(listView: ShadowListKitListView) {
        ends += 1
      }
    }
    list.dataSource = rows
    list.reloadData()
    assertThrows(IllegalStateException::class.java) { layOut(list) }
    assertEquals(0, ends)
    throws = false
    layOut(list)
    assertEquals(1, ends)
    layOut(list)
    assertEquals(1, ends)
  }

  @Test
  fun a_waiting_saved_position_is_dropped_by_a_scroll_command() = onMain {
    val rows = Rows(100, ROW_SIZE)
    val list = makeList(rows)
    list.restoreAnchorState(ShadowListKitAnchorState("later5", 4f))
    layOut(list)
    list.scrollToItem(50)
    layOut(list)
    val top = requireNotNull(list.cellForItem(50)).top - list.scrollY
    rows.keys.addAll(List(50) { "later$it" })
    list.reloadData()
    layOut(list)
    assertEquals(top, requireNotNull(list.cellForItem(50)).top - list.scrollY)
  }

  // region Header, footer and empty view

  @Test
  fun a_pinned_header_stays_at_the_viewport_start_and_a_pinned_footer_at_its_end() = onMain {
    val rows = SizedRows(100, 100.0)
    val list = ShadowListKitListView(context)
    val header = template(120)
    val footer = template(100)
    list.headerView = header
    list.footerView = footer
    list.stickyHeader = true
    list.stickyFooter = true
    makeSizedList(rows, list)
    assertEquals(120 + 100 * 100 + 100, list.contentSize)
    for (offset in intArrayOf(0, 40, 3000)) {
      list.scrollTo(0, offset)
      assertEquals(0, header.top - list.scrollY)
      assertEquals(HEIGHT, footer.bottom - list.scrollY)
    }
    // The rows keep their places under the two.
    assertEquals(120f, list.rectForItem(0)!!.top)
    scrollThrough(list, Int.MAX_VALUE)
    assertEquals(requireNotNull(list.cellForItem(99)).bottom, footer.top)

    list.stickyHeader = false
    list.stickyFooter = false
    layOut(list)
    assertEquals(0, header.top)
    assertEquals(list.contentSize, footer.bottom)
  }

  @Test
  fun auto_hide_bars_follow_user_moves_and_rebase_commands_without_a_core_pass() = onMain {
    val list = ShadowListKitListView(context)
    val header = template(120)
    val footer = template(100)
    list.headerView = header
    list.footerView = footer
    list.autoHideHeader = true
    list.autoHideFooter = true
    makeSizedList(SizedRows(100, 100.0), list)
    list.scrollDriven = true
    list.scrollTo(0, 1050)
    // As in the driven-host baseline, the pass after a jump publishes its reusable band.
    list.adjustOffsetBy(1.0)
    assertEquals(0, header.top - list.scrollY)
    val passes = list.corePassCount
    list.writeOffset(1081, byUser = true)
    list.layoutPass()
    assertEquals(-30, header.top - list.scrollY)
    assertEquals(HEIGHT + 30, footer.bottom - list.scrollY)
    assertEquals(passes, list.corePassCount)
    // A no-op layout cannot accumulate the same user move a second time.
    list.layoutPass()
    assertEquals(-30, header.top - list.scrollY)
    list.scrollTo(0, 1500)
    assertEquals(-30, header.top - list.scrollY)
    list.writeOffset(1480, byUser = true)
    list.layoutPass()
    assertEquals(-10, header.top - list.scrollY)
    assertEquals(HEIGHT + 10, footer.bottom - list.scrollY)
    // A no-op user write must not mark a later command as user movement.
    list.writeOffset(1480, byUser = true)
    list.scrollTo(0, 1700)
    assertEquals(-10, header.top - list.scrollY)
  }

  @Test
  fun a_user_move_then_a_correction_inside_one_mount_pass_accumulates_only_the_user_move() = onMain {
    val list = ShadowListKitListView(context)
    val header = template(120)
    list.headerView = header
    list.autoHideHeader = true
    val rows = SizedRows(100, 100.0)
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    var armed = false
    list.dataSource = object : ShadowListKitListView.DataSource, ShadowListKitListView.Sizing {
      override fun numberOfItems(listView: ShadowListKitListView) = rows.keys.size
      override fun keyForItem(listView: ShadowListKitListView, index: Int) = rows.keys[index]
      override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int) = 100
      override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell {
        if (armed) {
          armed = false
          listView.writeOffset(4030, byUser = true)
          listView.writeOffset(4500)
        }
        return listView.dequeueReusableCell("row")
      }
    }
    list.reloadData()
    layOut(list)
    list.scrollTo(0, 1000)
    armed = true
    // New mounted cells invoke the callback while layoutPass suppresses recursive scroll passes.
    list.scrollTo(0, 4000)
    assertFalse(armed)
    list.layoutPass()
    assertEquals(-30, header.top - list.scrollY)
  }

  @Test
  fun auto_hide_state_survives_flag_and_template_changes_and_resets_near_the_edge() = onMain {
    val list = ShadowListKitListView(context)
    var header = template(120)
    list.headerView = header
    list.autoHideHeader = true
    makeSizedList(SizedRows(100, 100.0), list)
    list.scrollTo(0, 1000)
    list.writeOffset(1040, byUser = true)
    list.layoutPass()
    assertEquals(-40, header.top - list.scrollY)
    list.autoHideHeader = false
    layOut(list)
    assertEquals(0, header.top)
    list.autoHideHeader = true
    layOut(list)
    assertEquals(-40, header.top - list.scrollY)
    list.headerView = null
    layOut(list)
    header = template(120)
    list.headerView = header
    layOut(list)
    assertEquals(-40, header.top - list.scrollY)
    list.scrollTo(0, 40)
    assertEquals(0, header.top - list.scrollY)
  }

  @Test
  fun horizontal_auto_hide_uses_the_scroll_axis_in_both_layout_directions() = onMain {
    for (inverted in listOf(false, true)) {
      val list = ShadowListKitListView(context)
      list.horizontal = true
      list.inverted = inverted
      val header = View(context).apply { minimumWidth = 120 }
      list.headerView = header
      list.autoHideHeader = true
      makeSizedList(SizedRows(100, 100.0), list)
      list.scrollTo(1000, 0)
      list.writeOffset(1020, byUser = true)
      list.layoutPass()
      assertEquals(-20, header.left - list.scrollX)
    }
  }

  @Test
  fun the_footer_pushes_a_pinned_header_off_and_a_pinned_footer_stays_under_the_header() = onMain {
    val list = ShadowListKitListView(context)
    val header = template(120)
    val footer = template(100)
    list.headerView = header
    list.footerView = footer
    list.stickyHeader = true
    list.registerCell("row") { ShadowListKitListCell(it, "row") }
    list.dataSource = SizedRows(100, 100.0)
    list.reloadData()
    // A viewport shorter than the two.
    layOut(list, 200)
    list.scrollTo(0, Int.MAX_VALUE)
    assertEquals(list.contentSize, footer.bottom)
    assertEquals(footer.top, header.bottom)

    list.stickyHeader = false
    list.stickyFooter = true
    list.scrollTo(0, 0)
    layOut(list, 200)
    assertEquals(header.bottom, footer.top)
  }

  @Test
  fun a_pinned_footer_of_a_short_list_rests_at_the_viewport_end() = onMain {
    val list = ShadowListKitListView(context)
    val footer = template(100)
    list.footerView = footer
    list.stickyFooter = true
    makeSizedList(SizedRows(2, 100.0), list)
    assertEquals(300, list.contentSize)
    assertEquals(HEIGHT, footer.bottom)
  }

  @Test
  fun a_pinned_header_and_footer_draw_over_the_rows_and_take_the_taps_there() = onMain {
    val list = OrderedListView(context)
    val header = template(120)
    val footer = template(100)
    list.headerView = header
    list.footerView = footer
    makeSizedList(SizedRows(100, 100.0), list)
    list.scrollTo(0, 1000)
    draw(list)
    assertEquals(listOf(footer, header), list.drawnChildren().take(2))

    list.stickyHeader = true
    list.stickyFooter = true
    layOut(list)
    draw(list)
    val drawn = list.drawnChildren()
    assertEquals(listOf(header, footer), drawn.takeLast(2))
    assertEquals(list.childCount, drawn.toSet().size)
    // A tap on the pinned header selects no row under it.
    list.handleTap(WIDTH / 2f, 60f)
    list.handleTap(WIDTH / 2f, HEIGHT - 50f)
    assertTrue(list.selectedIndices.isEmpty())
    list.handleTap(WIDTH / 2f, 170f)
    assertEquals(listOf(10), list.selectedIndices.toList())
  }

  @Test
  fun the_empty_view_shows_after_the_header_while_the_list_has_no_items() = onMain {
    val rows = SizedRows(0, 100.0)
    val list = ShadowListKitListView(context)
    val header = template(120)
    val footer = template(100)
    val empty = template(400)
    list.headerView = header
    list.footerView = footer
    list.emptyView = empty
    makeSizedList(rows, list)
    assertEquals(View.VISIBLE, empty.visibility)
    assertEquals(120, empty.top)
    assertEquals(520, empty.bottom)
    assertEquals(520, footer.top)
    assertEquals(620, list.contentSize)

    rows.keys.addAll(listOf("row0", "row1"))
    rows.sizes.addAll(listOf(100.0, 100.0))
    list.insertItems(intArrayOf(0, 1))
    layOut(list)
    assertEquals(View.INVISIBLE, empty.visibility)
    assertEquals(120f, list.rectForItem(0)!!.top)
    assertEquals(320, footer.top)
    assertEquals(420, list.contentSize)

    rows.keys.clear()
    rows.sizes.clear()
    list.reloadData()
    layOut(list)
    assertEquals(View.VISIBLE, empty.visibility)
    assertEquals(520, footer.top)

    list.emptyView = null
    layOut(list)
    assertNull(empty.parent)
    assertEquals(120, footer.top)
    assertEquals(220, list.contentSize)
  }

  @Test
  fun an_empty_view_set_on_a_list_with_items_stays_hidden() = onMain {
    val list = makeSizedList(SizedRows(5, 100.0))
    val empty = template(400)
    list.emptyView = empty
    layOut(list)
    assertEquals(View.INVISIBLE, empty.visibility)
    assertEquals(500, list.contentSize)
  }

  // endregion

  // region Anchors and the end

  @Test
  fun a_non_anchor_item_never_holds_the_visible_items() = onMain {
    for (nonAnchor in booleanArrayOf(false, true)) {
      val rows = SizedRows(100, 100.0)
      val list = makeSizedList(rows)
      if (nonAnchor) list.nonAnchorKeys = setOf("row10")
      list.scrollTo(0, 1000)
      assertEquals(0, requireNotNull(list.cellForItem(10)).top - list.scrollY)
      // The item at the viewport start grows.
      rows.sizes[10] = 160.0
      list.reloadItems(intArrayOf(10))
      layOut(list)
      if (nonAnchor) {
        // The item after it holds the content.
        assertEquals(100, requireNotNull(list.cellForItem(11)).top - list.scrollY)
      } else {
        assertEquals(0, requireNotNull(list.cellForItem(10)).top - list.scrollY)
      }
    }
  }

  @Test
  fun an_inverted_list_says_when_it_leaves_its_end_and_is_back() = onMain {
    val recorder = EndRecorder()
    val rows = SizedRows(200, 100.0)
    val parent = FrameLayout(context)
    val list = ShadowListKitListView(context)
    parent.addView(list)
    list.inverted = true
    list.delegate = recorder
    makeSizedList(rows, list)
    layOut(parent)
    val end = list.contentSize - HEIGHT
    assertEquals(end, list.scrollY)
    assertTrue(list.atEnd)

    // Items appended while the list rests at its end do not leave it.
    rows.keys.addAll(listOf("new0", "new1"))
    rows.sizes.addAll(listOf(100.0, 100.0))
    list.insertItems(intArrayOf(200, 201))
    layOut(parent)
    assertTrue(list.atEnd)
    assertTrue(recorder.changes.isEmpty())

    drag(parent, 300f, floatArrayOf(400f, 800f, 1300f, 1800f))
    assertTrue(list.scrollY < end - 1000)
    assertFalse(list.atEnd)
    assertEquals(listOf(false), recorder.changes)

    list.scrollToEnd()
    layOut(parent)
    assertTrue(list.atEnd)
    assertEquals(listOf(false, true), recorder.changes)

    // A scroll command that lands away from the end leaves it.
    list.scrollToItem(20)
    layOut(parent)
    assertFalse(list.atEnd)
    assertEquals(listOf(false, true, false), recorder.changes)
  }

  @Test
  fun a_list_that_is_not_inverted_is_always_at_its_end() = onMain {
    val recorder = EndRecorder()
    val parent = FrameLayout(context)
    val list = ShadowListKitListView(context)
    parent.addView(list)
    list.delegate = recorder
    makeSizedList(SizedRows(200, 100.0), list)
    layOut(parent)
    drag(parent, 1800f, floatArrayOf(1700f, 1300f, 800f, 300f))
    assertTrue(list.scrollY > 1000)
    assertTrue(list.atEnd)
    assertTrue(recorder.changes.isEmpty())
  }

  // endregion

  // region Long press and subclasses

  @Test
  fun a_long_press_goes_to_the_delegate_which_says_whether_it_took_it() = onMain {
    val pressed = ArrayList<Int>()
    var takes = true
    val list = makeList(Rows(100, ROW_SIZE))
    val cell = requireNotNull(list.cellForItem(3))
    // A delegate that does not answer takes none.
    list.delegate = object : ShadowListKitListView.Delegate {}
    assertFalse(list.longPressCell(cell))
    list.delegate = object : ShadowListKitListView.Delegate {
      override fun didLongPressItem(listView: ShadowListKitListView, index: Int): Boolean {
        pressed.add(index)
        return takes
      }
    }
    assertTrue(list.longPressCell(cell))
    takes = false
    assertFalse(list.longPressCell(cell))
    assertEquals(listOf(3, 3), pressed)
  }

  @Test
  fun a_subclass_guards_its_data_source_and_delegate_and_follows_editing() = onMain {
    val rows = Rows(50, ROW_SIZE)
    val list = GuardedListView(context)
    val delegate = object : ShadowListKitListView.Delegate {}
    list.delegate = delegate
    makeList(rows, list)
    list.locked = true
    list.dataSource = Rows(3, ROW_SIZE)
    list.delegate = null
    assertEquals(2, list.refused)
    assertTrue(list.dataSource === rows)
    assertTrue(list.delegate === delegate)
    list.reloadData()
    layOut(list)
    assertEquals((50 * ROW_SIZE).roundToInt(), list.contentSize)

    list.editing = true
    list.setEditing(false, true)
    assertEquals(listOf(true, false), list.editingCalls)
    assertFalse(list.editing)
  }

  // endregion

  // region Text

  @Test
  fun a_text_layout_gives_the_part_of_each_line_its_text_takes() {
    val paint = TextPaint().apply { textSize = 40f }
    val layout = ShadowListKitTextLayout.make("One\n\nTwo words", paint, 600)
    assertEquals(3, layout.lineCount)
    val first = requireNotNull(layout.rectForLine(0))
    val blank = requireNotNull(layout.rectForLine(1))
    val last = requireNotNull(layout.rectForLine(2))
    assertEquals(0f, first.left, 0f)
    assertEquals(0f, first.top, 0f)
    assertEquals(first.bottom, blank.top, 0f)
    assertEquals(0f, blank.width(), 0f)
    assertTrue(last.width() > first.width())
    assertEquals(layout.height.toFloat(), last.bottom, 0f)
    assertEquals(layout.width, ceil(last.width()).toInt())
    // The space a line ends in is not part of it.
    val spaced = ShadowListKitTextLayout.make("Two words   ", paint, 600)
    assertEquals(last.width(), requireNotNull(spaced.rectForLine(0)).width(), 0f)
    assertNull(layout.rectForLine(3))
    assertNull(layout.rectForLine(-1))
  }

  // endregion

  // region Swipe actions

  @Test
  fun a_swipe_action_without_a_color_takes_the_cores_gray_or_red() {
    val normal = ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.NORMAL, "Archive") { _, completion -> completion(true) }
    val destructive = ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.DESTRUCTIVE, "Delete") { _, completion -> completion(true) }
    assertEquals(0xFF8E8E93.toInt(), normal.shownColor)
    assertEquals(0xFFFF3B30.toInt(), destructive.shownColor)
    assertEquals(ShadowListKitCore.SWIPE_ACTION_COLOR_NORMAL, normal.shownColor)
    assertEquals(ShadowListKitCore.SWIPE_ACTION_COLOR_DESTRUCTIVE, destructive.shownColor)
    destructive.backgroundColor = 0xFF112233.toInt()
    assertEquals(0xFF112233.toInt(), destructive.shownColor)
  }

  // endregion
  @Test
  fun a_dropped_anchor_row_keeps_the_offset_during_the_delegate_move() = onMain {
    val rows = Rows(80, ROW_SIZE)
    val list = makeList(rows)
    list.scrollTo(0, (10 * ROW_SIZE + 7).roundToInt())
    val before = list.scrollY
    list.delegate = object : ShadowListKitListView.Delegate {
      override fun moveItem(listView: ShadowListKitListView, sourceIndex: Int, destinationIndex: Int) {
        rows.keys.add(destinationIndex, rows.keys.removeAt(sourceIndex))
      }
    }
    assertTrue(list.commitMove(10, 14))
    assertEquals(before, list.scrollY)
  }

  @Test
  fun a_rejected_drop_does_not_change_a_later_sort() = onMain {
    val rows = Rows(80, ROW_SIZE)
    val list = makeList(rows)
    list.scrollTo(0, (10 * ROW_SIZE + 7).roundToInt())
    list.delegate = object : ShadowListKitListView.Delegate {}
    assertTrue(list.commitMove(10, 14))
    val before = list.scrollY
    rows.keys.add(14, rows.keys.removeAt(10))
    list.reloadData()
    layOut(list)
    assertEquals((before + 4 * ROW_SIZE).roundToInt(), list.scrollY)
  }

  @Test
  fun a_delegate_that_throws_does_not_leave_a_drop_hint() = onMain {
    val rows = Rows(80, ROW_SIZE)
    val list = makeList(rows)
    list.scrollTo(0, (10 * ROW_SIZE + 7).roundToInt())
    list.delegate = object : ShadowListKitListView.Delegate {
      override fun moveItem(listView: ShadowListKitListView, sourceIndex: Int, destinationIndex: Int) {
        throw IllegalStateException("rejected move")
      }
    }
    assertThrows(IllegalStateException::class.java) { list.commitMove(10, 14) }
    val before = list.scrollY
    rows.keys.add(14, rows.keys.removeAt(10))
    list.reloadData()
    layOut(list)
    assertEquals((before + 4 * ROW_SIZE).roundToInt(), list.scrollY)
  }

}
