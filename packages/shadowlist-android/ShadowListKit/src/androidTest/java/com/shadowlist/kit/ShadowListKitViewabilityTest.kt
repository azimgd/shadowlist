package com.shadowlist.kit

import android.app.Activity
import android.content.Intent
import android.os.SystemClock
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ShadowListKitViewabilityTest {
  private open class Rows(val heights: MutableList<Int> = MutableList(20) { 100 }) : ShadowListKitListView.DataSource, ShadowListKitListView.Sizing {
    val keys = MutableList(heights.size) { "public-$it" }
    override fun numberOfItems(listView: ShadowListKitListView): Int = heights.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = keys[index]
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell = listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = heights[index]
  }
  private class SectionRows(val stableSectionKeys: Boolean = false) : Rows(MutableList(4) { 40 }), ShadowListKitListView.Sections {
    var firstCount = 2
    override fun numberOfSections(listView: ShadowListKitListView): Int = 2
    override fun numberOfItemsInSection(listView: ShadowListKitListView, section: Int): Int = if (section == 0) firstCount else 2
    override fun keyForSection(listView: ShadowListKitListView, section: Int): String? = if (stableSectionKeys) "section-$section" else null
    override fun titleForHeaderInSection(listView: ShadowListKitListView, section: Int): String = "Section $section"
    override fun sizeForHeaderInSection(listView: ShadowListKitListView, section: Int, crossSize: Int): Int = 20
  }
  private val instrumentation = InstrumentationRegistry.getInstrumentation()
  private fun main(body: () -> Unit) = instrumentation.runOnMainSync(body)
  private fun fixture(rows: Rows = Rows(), body: (Activity, ShadowListKitListView) -> Unit) {
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(Intent().setClassName(context.packageName,
      ShadowListKitBoardTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    lateinit var list: ShadowListKitListView
    try {
      main {
        list = ShadowListKitListView(activity)
        list.registerCell("row") { ShadowListKitListCell(it, "row") }
        list.dataSource = rows
        val root = FrameLayout(activity)
        root.addView(list, FrameLayout.LayoutParams(200, 100))
        activity.setContentView(root)
        list.reloadData()
      }
      instrumentation.waitForIdleSync()
      body(activity, list)
    } finally {
      main { activity.finish() }
      instrumentation.waitForIdleSync()
    }
  }

  @Test fun pairs_override_single_and_equal_config_replacement_keeps_emitted_keys() = fixture { _, list ->
    main {
      var single = 0
      var first = 0
      var replacement = 0
      val config = ShadowListKitViewabilityConfig(itemVisiblePercentThreshold = 50.0)
      list.viewabilityConfigCallbackPairs = emptyList()
      list.viewabilityConfig = config
      list.onViewableItemsChanged = { single++ }
      assertEquals(0, single)
      list.viewabilityConfigCallbackPairs = listOf(ShadowListKitViewabilityConfigCallbackPair(config) { info ->
        first++
        assertTrue(info.viewableItems.all { it.key.startsWith("public-") && it.section == -1 })
      })
      assertEquals(1, first)
      list.viewabilityConfigCallbackPairs = listOf(ShadowListKitViewabilityConfigCallbackPair(config.copy()) { replacement++ })
      assertEquals(0, replacement)
      list.writeOffset(100)
      list.layoutPass()
      assertEquals(1, replacement)
      list.viewabilityConfigCallbackPairs = null
      assertEquals(0, single)
    }
  }

  @Test fun minimum_time_requires_interaction_and_programmatic_movement_does_not_unlock() = fixture { _, list ->
    val delivered = CountDownLatch(1)
    var interacted = 0L
    var elapsed = 0L
    main {
      list.viewabilityConfig = ShadowListKitViewabilityConfig(minimumViewTime = 100.0, waitForInteraction = true)
      list.onViewableItemsChanged = { elapsed = SystemClock.uptimeMillis() - interacted; delivered.countDown() }
      list.writeOffset(100)
      list.layoutPass()
    }
    assertFalse(delivered.await(140, TimeUnit.MILLISECONDS))
    main { interacted = SystemClock.uptimeMillis(); list.recordInteraction() }
    assertFalse(delivered.await(30, TimeUnit.MILLISECONDS))
    assertTrue(delivered.await(2, TimeUnit.SECONDS))
    assertTrue("minimumViewTime must elapse on the actual attached host", elapsed >= 100)
  }

  @Test fun replacement_and_detach_cancel_pending_delivery() = fixture { activity, list ->
    val delivered = CountDownLatch(1)
    main {
      list.viewabilityConfig = ShadowListKitViewabilityConfig(minimumViewTime = 80.0)
      list.onViewableItemsChanged = { delivered.countDown() }
      list.viewabilityConfig = ShadowListKitViewabilityConfig(minimumViewTime = 300.0, waitForInteraction = true)
    }
    assertFalse(delivered.await(120, TimeUnit.MILLISECONDS))
    main {
      list.recordInteraction()
      (list.parent as FrameLayout).removeView(list)
    }
    assertFalse(delivered.await(350, TimeUnit.MILLISECONDS))
    main {
      (activity.findViewById<android.view.ViewGroup>(android.R.id.content).getChildAt(0) as FrameLayout)
        .addView(list, FrameLayout.LayoutParams(200, 100))
    }
    instrumentation.waitForIdleSync()
    assertFalse(delivered.await(350, TimeUnit.MILLISECONDS))
    main { list.recordInteraction() }
    assertTrue(delivered.await(2, TimeUnit.SECONDS))
  }

  @Test fun callback_replacement_and_detach_do_not_dispatch_stale_followups() = fixture { _, list ->
    main {
      var first = 0
      var stale = 0
      list.viewabilityConfigCallbackPairs = listOf(
        ShadowListKitViewabilityConfigCallbackPair(ShadowListKitViewabilityConfig()) {
          first++
          list.reloadData()
          list.viewabilityConfigCallbackPairs = emptyList()
          (list.parent as FrameLayout).removeView(list)
        },
        ShadowListKitViewabilityConfigCallbackPair(ShadowListKitViewabilityConfig(itemVisiblePercentThreshold = 75.0)) { stale++ },
      )
      assertEquals(1, first)
      assertEquals(0, stale)
    }
  }

  @Test fun masonry_qualifies_each_row_instead_of_including_the_bounding_gap() = fixture(Rows(mutableListOf(100, 60, 50, 100, 100, 100))) { _, list ->
    main {
      list.numberOfColumns = 2
      list.reloadData()
      list.layoutPass()
      list.writeOffset(50)
      list.layoutPass()
      var keys = emptyList<String>()
      list.viewabilityConfig = ShadowListKitViewabilityConfig(itemVisiblePercentThreshold = 50.0)
      list.onViewableItemsChanged = { keys = it.viewableItems.map { token -> token.key } }
      assertTrue(keys.contains("public-0"))
      assertFalse(keys.contains("public-1"))
      assertTrue(keys.contains("public-2"))
    }
  }
  @Test fun sections_report_public_item_keys_and_previous_metadata_for_removed_tokens() = fixture(SectionRows()) { _, list ->
    main {
      var info: ShadowListKitViewableItemsChangedInfo? = null
      list.onViewableItemsChanged = { info = it }
      assertEquals(listOf("public-0", "public-1"), info!!.viewableItems.map { it.key })
      assertTrue(info!!.viewableItems.all { it.section == 0 })
      list.writeOffset(120)
      list.layoutPass()
      assertEquals(listOf("public-2", "public-3"), info!!.viewableItems.map { it.key })
      assertTrue(info!!.viewableItems.all { it.section == 1 })
      val removed = info!!.changed.filter { !it.isViewable }
      assertEquals(listOf(0, 1), removed.map { it.index })
      assertTrue(removed.all { it.section == 0 && it.key.startsWith("public-") })
    }
  }

  @Test fun removal_after_reorder_keeps_the_last_emitted_public_identity() {
    val source = SectionRows(stableSectionKeys = true)
    fixture(source) { _, list ->
      main {
        var info: ShadowListKitViewableItemsChangedInfo? = null
        list.onViewableItemsChanged = { info = it }
        java.util.Collections.swap(source.keys, 0, 1)
        list.reloadData()
        list.layoutPass()
        source.keys.removeAt(1)
        source.heights.removeAt(1)
        source.firstCount = 1
        list.reloadData()
        list.layoutPass()
        val removed = info!!.changed.filter { !it.isViewable }
        assertEquals(listOf("public-0"), removed.map { it.key })
        assertEquals(listOf(0), removed.map { it.index })
        assertEquals(listOf(0), removed.map { it.section })
        assertTrue(info!!.viewableItems.all { it.key.startsWith("public-") })
      }
    }
  }

  @Test fun user_scroll_entry_unlocks_waiting_without_a_programmatic_unlock() = fixture { _, list ->
    main {
      var calls = 0
      list.viewabilityConfig = ShadowListKitViewabilityConfig(waitForInteraction = true)
      list.onViewableItemsChanged = { calls++ }
      list.writeOffset(50)
      list.layoutPass()
      assertEquals(0, calls)
      list.scrollTrackingStarted()
      assertEquals(1, calls)
    }
  }

  @Test fun reload_only_delivers_the_immutable_frame_before_its_next_frame() {
    val source = Rows()
    fixture(source) { _, list ->
      main {
        val events = mutableListOf<String>()
        var reloaded = false
        val config = ShadowListKitViewabilityConfig(itemVisiblePercentThreshold = 50.0)
        list.viewabilityConfigCallbackPairs = listOf(
          ShadowListKitViewabilityConfigCallbackPair(config) { info ->
            events.add("first:" + info.viewableItems.joinToString(",") { it.key })
            if (!reloaded) {
              reloaded = true
              source.keys.removeAt(0)
              source.heights.removeAt(0)
              list.reloadData()
            }
          },
          ShadowListKitViewabilityConfigCallbackPair(config) { info ->
            events.add("second:" + info.viewableItems.joinToString(",") { it.key })
          },
        )
        list.layoutPass()
        assertEquals(listOf("first:public-0", "second:public-0", "first:public-1", "second:public-1"), events)
      }
    }
  }

}
