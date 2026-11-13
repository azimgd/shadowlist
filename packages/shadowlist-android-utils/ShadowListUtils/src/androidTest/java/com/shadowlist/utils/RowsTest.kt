package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration
import android.graphics.RectF
import android.graphics.drawable.ColorDrawable
import android.view.View.MeasureSpec
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.shadowlist.kit.ShadowListKitListView
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.atomic.AtomicInteger

/*
 * The base list, the row view and the layout cache: rows by key, content versions, sizes from
 * layouts, the closures, the theme and horizontal lists.
 */
@RunWith(AndroidJUnit4::class)
class RowsTest {
  private val width = TEST_WIDTH_DP.px
  private val rowHeight = 44f.px

  @Before
  fun clearFills() = onMain { CountingRowView.fills.clear() }

  private fun rows(count: Int): List<CountingRow> = List(count) { CountingRow("row-$it") }

  /*
   * A row a test gives its layout to, shown by the counting view.
   */
  private class LayoutRow(
    override val key: String,
    val tag: Int = 0,
    private val make: (widthPx: Int) -> ShadowListUtilsRowLayout,
  ) : ShadowListUtilsRow {
    override val reuseIdentifier: String get() = "LayoutRow"

    override fun makeView(context: Context): ShadowListUtilsRowView = CountingRowView(context, reuseIdentifier)

    override fun layout(widthPx: Int): ShadowListUtilsRowLayout = make(widthPx)
  }

  private fun frame(left: Int, top: Int, width: Int, height: Int) =
    RectF(left.toFloat(), top.toFloat(), (left + width).toFloat(), (top + height).toFloat())

  private fun backgroundColor(list: ShadowListUtilsList): Int = (list.background as ColorDrawable).color

  @Test
  fun the_list_shows_its_rows_in_order() = onMain {
    val list = ShadowListUtilsList(testContext)
    host(list)
    list.setRows(rows(5))
    host(list)
    assertEquals(listOf("row-0", "row-1", "row-2", "row-3", "row-4"), list.keys)
    assertTrue(list.rowView(0) is CountingRowView)
    assertEquals("row-3", list.rowView(3)?.row?.key)
    assertEquals(frame(0, 2 * rowHeight, width, rowHeight), list.rectForItem(2))
  }

  @Test
  fun a_row_with_a_new_content_version_is_the_only_one_configured_again() = onMain {
    val list = ShadowListUtilsList(testContext)
    host(list)
    val rows = rows(4).toMutableList()
    list.setRows(rows.toList())
    host(list)
    assertEquals(mapOf("row-0" to 1, "row-1" to 1, "row-2" to 1, "row-3" to 1), CountingRowView.fills)
    rows[2] = CountingRow("row-2", contentVersion = 7, height = 60f)
    list.setRows(rows.toList())
    host(list)
    assertEquals(mapOf("row-0" to 1, "row-1" to 1, "row-2" to 2, "row-3" to 1), CountingRowView.fills)
    assertEquals(60f.px.toFloat(), list.rectForItem(2)?.height())
    assertEquals((2 * rowHeight + 60f.px).toFloat(), list.rectForItem(3)?.top)
  }

  @Test
  fun a_deleted_row_leaves_and_an_inserted_row_arrives() = onMain {
    val list = ShadowListUtilsList(testContext)
    host(list)
    val rows = rows(4).toMutableList()
    list.setRows(rows.toList())
    host(list)
    rows.removeAt(1)
    rows.add(CountingRow("row-9"))
    list.setRows(rows.toList(), animated = true)
    host(list)
    assertEquals(listOf("row-0", "row-2", "row-3", "row-9"), list.keys)
    assertEquals("row-2", list.rowView(1)?.row?.key)
    assertEquals(1, CountingRowView.fills["row-9"])
    assertFalse(list.animatesChanges)
  }

  @Test
  fun the_closures_run_for_the_kits_delegate_calls() = onMain {
    val list = ShadowListUtilsList(testContext)
    host(list)
    list.setRows(rows(3))
    val calls = ArrayList<String>()
    list.onEndReached = { calls.add("end") }
    list.onStartReached = { calls.add("start") }
    list.onScroll = { calls.add("scroll") }
    list.onRefresh = { calls.add("refresh") }
    val delegate = requireNotNull(list.delegate) { "the list has no delegate" }
    delegate.didReachEnd(list)
    delegate.didReachStart(list)
    delegate.didScroll(list)
    delegate.didBeginRefreshing(list)
    assertEquals(listOf("end", "start", "scroll", "refresh"), calls)

    // The kit makes the calls itself when the list scrolls to its end.
    list.setRows(rows(200))
    host(list)
    calls.clear()
    list.scrollToEnd()
    host(list)
    assertTrue(calls.contains("end"))
    assertTrue(calls.contains("scroll"))
  }

  @Test
  fun on_refresh_turns_pull_to_refresh_on_and_off() = onMain {
    val list = ShadowListUtilsList(testContext)
    assertFalse(list.refreshEnabled)
    list.onRefresh = {}
    assertTrue(list.refreshEnabled)
    list.onRefresh = null
    assertFalse(list.refreshEnabled)
  }

  @Test
  fun the_list_is_its_own_data_source_and_delegate() = onMain {
    val list = ShadowListUtilsList(testContext)
    assertNotNull(list.dataSource)
    assertSame(list.dataSource, list.delegate)
    assertSame(list.adapter, list.dataSource)
    assertEquals(0, list.dataSource?.numberOfItems(list))
    // The test app is a debug build, which stops on another data source or delegate.
    assertThrows(IllegalStateException::class.java) {
      list.delegate = object : ShadowListKitListView.Delegate {}
    }
    assertThrows(IllegalStateException::class.java) {
      list.dataSource = ShadowListUtilsListAdapter(ShadowListUtilsList(testContext))
    }
    assertSame(list.adapter, list.delegate)
    assertSame(list.adapter, list.dataSource)
  }

  @Test
  fun the_theme_sets_the_background_and_configures_every_cell_again() = onMain {
    val list = ShadowListUtilsList(testContext, ShadowListUtilsTheme.LIGHT)
    host(list)
    list.setRows(rows(3))
    host(list)
    assertEquals(ShadowListUtilsTheme.LIGHT.colors.background, backgroundColor(list))
    list.theme = ShadowListUtilsTheme.DARK
    host(list)
    assertEquals(ShadowListUtilsTheme.DARK.colors.background, backgroundColor(list))
    assertEquals(mapOf("row-0" to 2, "row-1" to 2, "row-2" to 2), CountingRowView.fills)
  }

  @Test
  fun a_new_text_size_lays_every_row_out_again() = onMain {
    val body = ShadowListUtilsTheme.LIGHT.typography.body
    val started = AtomicInteger()
    val finished = AtomicInteger()
    fun textRow(key: String) = LayoutRow(key) {
      started.incrementAndGet()
      ShadowListUtilsRowLayout(it, body.layout("One line", it).height).also { finished.incrementAndGet() }
    }
    withTextSize(1f) {
      val list = ShadowListUtilsList(testContext)
      host(list)
      list.setRows(listOf(textRow("a"), textRow("b")))
      host(list)
      val plain = body.scaledLineHeightPx()
      assertEquals(frame(0, plain, width, plain), list.rectForItem(1))
      // A layout the cache's threads still work on at the size before ends first.
      while (started.get() != finished.get()) Thread.sleep(1)
      withTextSize(2f) {
        // What the system tells a view when the reader picks another text size.
        list.dispatchConfigurationChanged(Configuration(list.resources.configuration))
        host(list)
        val large = body.scaledLineHeightPx()
        assertTrue(large > plain)
        assertEquals(frame(0, large, width, large), list.rectForItem(1))
        assertEquals(mapOf("a" to 2, "b" to 2), CountingRowView.fills)
      }
    }
  }

  @Test
  fun a_narrower_list_lays_its_rows_out_for_the_new_width() = onMain {
    fun widthRow(key: String) = LayoutRow(key) { ShadowListUtilsRowLayout(it, it / 10) }
    val list = ShadowListUtilsList(testContext)
    host(list)
    list.setRows(listOf(widthRow("a"), widthRow("b")))
    host(list)
    assertEquals(frame(0, width / 10, width, width / 10), list.rectForItem(1))
    val narrow = 300f.px
    host(list, widthPx = narrow)
    assertEquals(frame(0, narrow / 10, narrow, narrow / 10), list.rectForItem(1))
  }

  @Test
  fun a_horizontal_list_reads_a_layouts_width_as_the_size_along_its_axis() = onMain {
    val along = 120f.px
    val cross = 200f.px
    val list = ShadowListUtilsList(testContext)
    list.horizontal = true
    host(list, heightPx = cross)
    list.setRows(List(4) { LayoutRow("wide-$it") { crossPx -> ShadowListUtilsRowLayout(along, crossPx) } })
    host(list, heightPx = cross)
    assertEquals(frame(2 * along, 0, along, cross), list.rectForItem(2))
    assertEquals(along, list.rowView(1)?.width)
    assertEquals(cross, list.rowView(1)?.height)
  }

  @Test
  fun a_horizontal_row_view_keeps_the_layout_of_its_cross_size() = onMain {
    val along = 120f.px
    val cross = 200f.px
    val layouts = AtomicInteger()
    val list = ShadowListUtilsList(testContext)
    list.horizontal = true
    host(list, heightPx = cross)
    // The content is shorter than the list is tall.
    list.setRows(
      List(2) {
        LayoutRow("short-$it") { crossPx ->
          layouts.incrementAndGet()
          ShadowListUtilsRowLayout(along, crossPx / 2)
        }
      })
    host(list, heightPx = cross)
    // The prefetch can lay a row out beside the list. It is over before the count is taken.
    list.layoutCache.cancelPrefetch()
    Thread.sleep(PREFETCH_SETTLE_MS)
    val laidOut = layouts.get()
    assertTrue(laidOut >= 2)
    val view = requireNotNull(list.rowView(0)) { "item 0 is not mounted" }
    view.requestLayout()
    view.measure(
      MeasureSpec.makeMeasureSpec(view.width, MeasureSpec.EXACTLY),
      MeasureSpec.makeMeasureSpec(view.height, MeasureSpec.EXACTLY))
    view.layout(view.left, view.top, view.right, view.bottom)
    assertEquals(laidOut, layouts.get())
    val second = requireNotNull(list.rowView(1)) { "item 1 is not mounted" }
    assertEquals(listOf(along, 0, 2 * along, cross), listOf(second.left, second.top, second.right, second.bottom))
  }

  @Test
  fun a_row_view_without_a_layout_sizes_itself_from_its_row() = onMain {
    val view = CountingRowView(testContext, "CountingRow")
    view.configure(CountingRow("self-sized", height = 72f), null)
    view.measure(
      MeasureSpec.makeMeasureSpec(320f.px, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
    assertEquals(320f.px, view.measuredWidth)
    assertEquals(72f.px, view.measuredHeight)
    view.prepareForReuse()
    assertNull(view.row)
  }

  @Test
  fun a_mounted_cell_takes_a_new_row_with_the_same_content_version() = onMain {
    fun taggedRow(tag: Int) = LayoutRow("a", tag) { ShadowListUtilsRowLayout(it, rowHeight) }
    val list = ShadowListUtilsList(testContext)
    host(list)
    list.setRows(listOf(taggedRow(1)))
    host(list)
    list.setRows(listOf(taggedRow(2)))
    assertEquals(2, (list.rowView(0)?.row as? LayoutRow)?.tag)
    assertEquals(mapOf("a" to 1), CountingRowView.fills)
  }

  @Test
  fun an_append_inserts_its_rows_and_keeps_the_others() = onMain {
    val list = ShadowListUtilsList(testContext)
    host(list)
    val rows = rows(3).toMutableList()
    list.setRows(rows.toList())
    host(list)
    val first = list.rowView(0)
    rows.add(CountingRow("row-3"))
    list.setRows(rows.toList())
    host(list)
    assertEquals(frame(0, 3 * rowHeight, width, rowHeight), list.rectForItem(3))
    assertEquals("row-3", list.rowView(3)?.row?.key)
    rows.add(0, CountingRow("row-9"))
    list.setRows(rows.toList())
    host(list)
    assertEquals("row-9", list.rowView(0)?.row?.key)
    assertSame(first, list.rowView(1))
    assertEquals(mapOf("row-0" to 1, "row-1" to 1, "row-2" to 1, "row-3" to 1, "row-9" to 1), CountingRowView.fills)
    // A change next to the new rows is no pure append and goes through the diff.
    rows[1] = CountingRow("row-0", contentVersion = 5)
    rows.add(CountingRow("row-4"))
    list.setRows(rows.toList())
    host(list)
    assertEquals(2, CountingRowView.fills["row-0"])
    assertEquals(6, list.keys.size)
  }

  @Test
  fun the_cache_keeps_a_layout_per_key_width_and_content_version() {
    val cache = ShadowListUtilsLayoutCache()
    val row = CountingRow("cached")
    val first = cache.layout(row, 300)
    assertSame(first, cache.layout(row, 300))
    assertNotSame(first, cache.layout(row, 200))
    val narrow = cache.layout(row, 200)
    val changed = CountingRow("cached", contentVersion = 1)
    assertNotSame(narrow, cache.layout(changed, 200))
    val current = cache.layout(changed, 200)
    assertSame(current, cache.layout(changed, 200))
    cache.invalidate("cached")
    assertNotSame(current, cache.layout(changed, 200))
    val last = cache.layout(changed, 200)
    cache.invalidateAll()
    assertNotSame(last, cache.layout(changed, 200))
  }

  @Test
  fun the_cache_drops_the_layouts_of_keys_that_left_the_rows() = onMain {
    val list = ShadowListUtilsList(testContext)
    host(list)
    list.setRows(rows(6))
    host(list)
    assertEquals(6, list.layoutCache.count)
    list.setRows(rows(6).takeLast(2))
    host(list)
    assertEquals(2, list.layoutCache.count)
  }

  @Test
  fun a_layout_made_while_layouts_were_dropped_is_not_kept() {
    // A row whose layout drops the cache's layouts, as a text size change does while a prefetch is in flight.
    val cache = ShadowListUtilsLayoutCache()
    val dropping = LayoutRow("dropped") { widthPx ->
      cache.invalidateAll()
      ShadowListUtilsRowLayout(widthPx, 10)
    }
    val stale = cache.layout(dropping, 300)
    assertEquals(0, cache.count)
    val kept = cache.layout(CountingRow("dropped"), 300)
    assertNotSame(stale, kept)
    assertEquals(1, cache.count)
    assertSame(kept, cache.layout(CountingRow("dropped"), 300))
  }

  @Test
  fun prefetch_lays_rows_out_ahead_on_its_queue() {
    val layouts = AtomicInteger()
    val cache = ShadowListUtilsLayoutCache()
    val rows = List(20) {
      LayoutRow("row-$it") { widthPx ->
        layouts.incrementAndGet()
        ShadowListUtilsRowLayout(widthPx, 10)
      }
    }
    cache.prefetch(rows, 300, around = 10)
    val deadline = System.currentTimeMillis() + 5_000
    while (layouts.get() < rows.size && System.currentTimeMillis() < deadline) Thread.sleep(10)
    assertEquals(rows.size, layouts.get())
    cache.layout(rows[3], 300)
    assertEquals(rows.size, layouts.get())
  }

  @Test
  fun a_moved_item_lands_at_its_destination() {
    assertEquals(listOf("b", "c", "a", "d"), ShadowListUtilsArrayMove.moved(listOf("a", "b", "c", "d"), 0, 2))
    assertEquals(listOf("a", "d", "b", "c"), ShadowListUtilsArrayMove.moved(listOf("a", "b", "c", "d"), 3, 1))
    assertEquals(listOf("a", "b"), ShadowListUtilsArrayMove.moved(listOf("a", "b"), 1, 1))
    assertEquals(listOf("a", "b"), ShadowListUtilsArrayMove.moved(listOf("a", "b"), 0, 5))
  }

  private companion object {
    const val PREFETCH_SETTLE_MS = 100L
  }
}
