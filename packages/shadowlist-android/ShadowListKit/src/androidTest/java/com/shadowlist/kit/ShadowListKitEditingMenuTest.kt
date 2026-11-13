package com.shadowlist.kit

import android.content.Intent
import android.os.SystemClock
import android.view.InputDevice
import android.view.Menu
import android.view.MotionEvent
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ShadowListKitEditingMenuTest {
  private class Rows(val offerMenu: Boolean) : ShadowListKitListView.DataSource,
    ShadowListKitListView.Sizing, ShadowListKitListView.Delegate {
    val held = CountDownLatch(1)
    var menus = 0
    var longPresses = 0
    var selections = 0
    override fun numberOfItems(listView: ShadowListKitListView): Int = 10
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = "item-$index"
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell = listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = 40
    override fun contextMenuForItem(listView: ShadowListKitListView, index: Int, menu: Menu): Boolean {
      menus++
      assertEquals(0, index)
      if (offerMenu) menu.add("Inspect")
      held.countDown()
      return offerMenu
    }
    override fun didLongPressItem(listView: ShadowListKitListView, index: Int): Boolean {
      longPresses++
      held.countDown()
      return !offerMenu
    }
    override fun didSelectItem(listView: ShadowListKitListView, index: Int) { selections++ }
  }

  private fun hold(editing: Boolean, offerMenu: Boolean) {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(Intent().setClassName(context.packageName,
      ShadowListKitBoardTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    val rows = Rows(offerMenu)
    lateinit var list: ShadowListKitListView
    var downTime = 0L
    fun touch(action: Int) {
      val event = MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, 100f, 20f, 0)
      event.source = InputDevice.SOURCE_TOUCHSCREEN
      try { list.dispatchTouchEvent(event) } finally { event.recycle() }
    }
    try {
      instrumentation.runOnMainSync {
        list = ShadowListKitListView(activity)
        list.registerCell("row") { ShadowListKitListCell(it, "row") }
        list.dataSource = rows
        list.delegate = rows
        list.reorderEnabled = editing
        list.editing = editing
        val root = FrameLayout(activity)
        root.addView(list, FrameLayout.LayoutParams(300, 300))
        activity.setContentView(root)
        list.reloadData()
      }
      instrumentation.waitForIdleSync()
      instrumentation.runOnMainSync {
        assertNotNull(list.windowToken)
        assertNotNull(list.cellForItem(0))
        downTime = SystemClock.uptimeMillis()
        touch(MotionEvent.ACTION_DOWN)
      }
      // Observe the actual configured long-press callback, not a fixed delay or private hook.
      assertTrue("The attached list must dispatch its hold", rows.held.await(3, TimeUnit.SECONDS))
      instrumentation.runOnMainSync {
        touch(MotionEvent.ACTION_UP)
        assertEquals(1, rows.menus)
        assertEquals(if (offerMenu) 0 else 1, rows.longPresses)
        assertEquals(0, rows.selections)
        assertTrue(list.selectedIndices.isEmpty())
      }
    } finally {
      instrumentation.runOnMainSync { activity.finish() }
      instrumentation.waitForIdleSync()
    }
  }

  @Test fun a_supplied_menu_owns_the_hold_without_selecting() = hold(editing = false, offerMenu = true)
  @Test fun editing_disables_reorder_but_keeps_the_supplied_menu() = hold(editing = true, offerMenu = true)
  @Test fun a_declined_menu_preserves_the_consumed_long_press_while_editing() = hold(editing = true, offerMenu = false)
}
