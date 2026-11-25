package com.shadowlist.kit

import android.content.Intent
import android.os.SystemClock
import android.view.MotionEvent
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class ShadowListKitSwipePolicyTest {
  private open class Rows :
    ShadowListKitListView.DataSource,
    ShadowListKitListView.Sizing,
    ShadowListKitListView.Delegate {
    var key = "reservation"
    var leadingCalls = 0
    var trailingCalls = 0
    var performed = 0
    var provider: ((ShadowListKitListView) -> Unit)? = null
    override fun numberOfItems(listView: ShadowListKitListView): Int = 1
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = key
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
      listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = 100
    override fun leadingSwipeActionsForItem(
      listView: ShadowListKitListView,
      index: Int
    ): ShadowListKitSwipeActionsConfiguration {
      leadingCalls++
      provider?.invoke(listView)
      return ShadowListKitSwipeActionsConfiguration(
        listOf(
          ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.NORMAL, "Archive") {
              _,
              done
            ->
            performed++
            done(true)
          }
        )
      )
    }
    override fun trailingSwipeActionsForItem(
      listView: ShadowListKitListView,
      index: Int
    ): ShadowListKitSwipeActionsConfiguration? {
      trailingCalls++
      return null
    }
  }

  private class SectionRows :
    Rows(),
    ShadowListKitListView.Sections {
    override fun numberOfSections(listView: ShadowListKitListView): Int = 1
    override fun numberOfItemsInSection(listView: ShadowListKitListView, section: Int): Int = 1
    override fun titleForHeaderInSection(listView: ShadowListKitListView, section: Int): String = "Reservations"
    override fun sizeForHeaderInSection(listView: ShadowListKitListView, section: Int, crossSize: Int): Int = 20
  }

  private fun attached(rows: Rows = Rows(), body: (ShadowListKitListView, ShadowListKitListCell, Rows) -> Unit) {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val activity = instrumentation.startActivitySync(
      Intent().setClassName(
        instrumentation.context.packageName,
        ShadowListKitBoardTestActivity::class.java.name
      ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    )
    lateinit var list: ShadowListKitListView
    try {
      instrumentation.runOnMainSync {
        list = ShadowListKitListView(activity)
        list.registerCell("row") { ShadowListKitListCell(it, "row") }
        list.dataSource = rows
        list.delegate = rows
        val root = FrameLayout(activity)
        root.addView(list, FrameLayout.LayoutParams(300, 300))
        activity.setContentView(root)
        list.reloadData()
      }
      instrumentation.waitForIdleSync()
      instrumentation.runOnMainSync { body(list, requireNotNull(list.cellForItem(0)), rows) }
    } finally {
      instrumentation.runOnMainSync { activity.finish() }
    }
  }

  @Test fun accessibility_uses_the_existing_editing_and_held_policy() = attached { list, cell, rows ->
    list.editing = true
    assertTrue(list.swipe.accessibilityActions(cell).isEmpty())
    assertFalse(list.swipe.performAccessibilityAction(cell, 0))
    assertEquals(0, rows.leadingCalls)
    list.editing = false
    assertEquals("Archive", list.swipe.accessibilityActions(cell).single().title)
    list.reorderEnabled = true
    assertTrue(list.beginDragAt(100f, 20f))
    assertTrue(list.hasHeldRow)
    assertTrue(list.swipe.accessibilityActions(cell).isEmpty())
    assertFalse(list.swipe.performAccessibilityAction(cell, 0))
    assertEquals(0, rows.performed)
    list.drag.reset()
  }

  @Test fun provider_reload_or_detach_cannot_resolve_an_action_for_the_old_item() {
    attached { list, cell, rows ->
      rows.provider = { current ->
        rows.key = "replacement"
        current.reloadData()
      }
      assertTrue(list.swipe.accessibilityActions(cell).isEmpty())
      assertFalse(list.swipe.performAccessibilityAction(cell, 0))
      assertEquals(0, rows.trailingCalls)
      assertEquals(0, rows.performed)
    }
    // A replacement mounts on a later pass. Detach gets a separate genuinely mounted fixture.
    attached { list, cell, rows ->
      rows.provider = { view -> (view.parent as FrameLayout).removeView(view) }
      assertFalse(list.swipe.performAccessibilityAction(cell, 0))
      assertEquals(0, rows.trailingCalls)
      assertEquals(0, rows.performed)
    }
  }

  @Test fun provider_rebinding_during_touch_never_inserts_an_action_surface() = attached { list, _, rows ->
    rows.provider = { current ->
      rows.key = "replacement"
      current.reloadData()
    }
    val down = SystemClock.uptimeMillis()
    for ((action, x) in listOf(MotionEvent.ACTION_DOWN to 50f, MotionEvent.ACTION_MOVE to 150f)) {
      val event = MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, x, 30f, 0)
      try {
        list.swipe.handle(event)
      } finally {
        event.recycle()
      }
    }
    assertFalse(list.swipe.isOpen)
    assertEquals(1, rows.leadingCalls)
    assertEquals(0, rows.trailingCalls)
    assertEquals(0, rows.performed)
    assertFalse((0 until list.childCount).any { list.getChildAt(it) is ShadowListKitSwipeActionsView })
  }

  @Test fun section_headers_never_resolve_item_swipe_configuration() = attached(SectionRows()) { list, _, rows ->
    val header = list.mounted.values.first {
      it.index == ShadowListKitListCell.NO_INDEX && list.itemForRow(it.row) == -1
    }
    assertSame(header, list.mounted[list.keys[header.row]])
    assertTrue(list.swipe.accessibilityActions(header).isEmpty())
    assertFalse(list.swipe.performAccessibilityAction(header, 0))
    assertEquals(0, rows.leadingCalls)
    assertEquals(0, rows.trailingCalls)
  }
}
