package com.shadowlist.kit

import android.content.Intent
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Button
import android.widget.FrameLayout
import androidx.core.view.ViewCompat
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Test

class ShadowListKitItemActionsTest {
  private fun attached(body: (ShadowListKitItemActionsView, Button) -> Unit) {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    instrumentation.uiAutomation
    val activity = instrumentation.startActivitySync(Intent().setClassName(instrumentation.context.packageName,
      ShadowListKitBoardTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    lateinit var actions: ShadowListKitItemActionsView
    lateinit var button: Button
    val focused = CountDownLatch(1)
    try {
      instrumentation.runOnMainSync {
        actions = ShadowListKitItemActionsView(activity)
        button = Button(activity).apply { text = "Reservation" }
        actions.contentView.addView(button, FrameLayout.LayoutParams(-1, -1))
        val root = FrameLayout(activity)
        root.addView(actions, FrameLayout.LayoutParams(300, 100))
        activity.setContentView(root)
        activity.window.decorView.viewTreeObserver.addOnWindowFocusChangeListener { if (it) focused.countDown() }
        if (activity.window.decorView.hasWindowFocus()) focused.countDown()
      }
      assertTrue("The actual test activity must own window focus", focused.await(5, TimeUnit.SECONDS))
      instrumentation.waitForIdleSync()
      assertTrue(actions.isAttachedToWindow)
      body(actions, button)
    } finally {
      instrumentation.runOnMainSync { activity.finish() }
      instrumentation.waitForIdleSync()
    }
  }

  private fun touch(view: ShadowListKitItemActionsView, action: Int, down: Long, x: Float, y: Float) {
    val event = MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, x, y, 0)
    try { view.dispatchTouchEvent(event) } finally { event.recycle() }
  }

  @Test fun reveal_owns_only_its_surface_and_cancel_preserves_caller_state() = attached { view, button ->
    InstrumentationRegistry.getInstrumentation().runOnMainSync {
      button.translationX = 7f; button.rotation = 3f; button.alpha = 0.8f
      val action = ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.NORMAL, "Archive") { _, done -> done(true) }
      view.leadingSwipeActions = ShadowListKitSwipeActionsConfiguration(listOf(action))
      val down = SystemClock.uptimeMillis()
      touch(view, MotionEvent.ACTION_DOWN, down, 80f, 50f)
      touch(view, MotionEvent.ACTION_MOVE, down, 170f, 50f)
      assertTrue(view.cell.translationX > 0)
      assertEquals(ShadowListKitListCell.NO_INDEX, view.cell.index)
      assertEquals(7f, button.translationX); assertEquals(3f, button.rotation); assertEquals(0.8f, button.alpha)
      touch(view, MotionEvent.ACTION_CANCEL, down, 170f, 50f)
      view.closeSwipeActions(false)
      assertEquals(0f, view.cell.translationX)
      assertEquals(7f, button.translationX); assertEquals(3f, button.rotation); assertEquals(0.8f, button.alpha)
    }
  }

  @Test fun native_accessibility_actions_preserve_caller_entries_and_read_current_configuration() = attached { view, _ ->
    InstrumentationRegistry.getInstrumentation().runOnMainSync {
      var caller = 0
      var archived = 0
      val callerId = ViewCompat.addAccessibilityAction(view, "Inspect") { _, _ -> caller++; true }
      view.trailingSwipeActions = ShadowListKitSwipeActionsConfiguration(listOf(
        ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.NORMAL, "Archive") { _, done -> archived++; done(true) }))
      val info = view.createAccessibilityNodeInfo()
      val archive = info.actionList.first { it.label == "Archive" }
      assertTrue(info.actionList.any { it.id == callerId && it.label == "Inspect" })
      assertTrue(view.performAccessibilityAction(archive.id, null))
      assertTrue(view.performAccessibilityAction(callerId, null))
      assertEquals(1, archived); assertEquals(1, caller)
      info.recycle()
      view.trailingSwipeActions = null
      assertFalse(view.performAccessibilityAction(archive.id, null))
    }
  }

  @Test fun actual_exposed_content_node_offers_swipe_and_later_caller_actions() = attached { view, button ->
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    var archived = 0
    instrumentation.runOnMainSync {
      button.accessibilityDelegate = object : View.AccessibilityDelegate() {
        override fun onInitializeAccessibilityNodeInfo(host: View, info: AccessibilityNodeInfo) {
          super.onInitializeAccessibilityNodeInfo(host, info)
          info.className = "com.example.Reservation"
        }
      }
      ViewCompat.addAccessibilityAction(button, "Inspect") { _, _ -> true }
      view.trailingSwipeActions = ShadowListKitSwipeActionsConfiguration(listOf(
        ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.NORMAL, "Archive") { _, done -> archived++; done(true) }))
    }
    instrumentation.waitForIdleSync()
    instrumentation.uiAutomation.waitForIdle(100, 3000)
    var reached = false
    var exposed: AccessibilityNodeInfo? = null
    fun visit(node: AccessibilityNodeInfo) {
      if (node.text?.toString()?.equals("Reservation", ignoreCase = true) == true) {
        reached = true
        exposed = AccessibilityNodeInfo.obtain(node)
        assertTrue("The real exposed content node must offer Archive", node.actionList.any { it.label == "Archive" })
        assertEquals(1, node.actionList.count { it.label == "Archive" })
        assertTrue(node.actionList.any { it.label == "Inspect" })
        assertEquals("com.example.Reservation", node.className)
      }
      for (index in 0 until node.childCount) {
        val child = node.getChild(index) ?: continue
        try { visit(child) } finally { child.recycle() }
      }
    }
    val root = instrumentation.uiAutomation.rootInActiveWindow
    assertNotNull(root)
    try { visit(root) } finally { root.recycle() }
    assertTrue("The actual native window must expose the content button", reached)
    val owner = requireNotNull(exposed)
    val archiveId = owner.actionList.first { it.label == "Archive" }.id
    assertTrue(owner.performAction(archiveId))
    instrumentation.runOnMainSync {
      assertEquals(1, archived)
      ViewCompat.addAccessibilityAction(button, "Share") { _, _ -> true }
      (view.parent as FrameLayout).removeView(view)
      val info = button.createAccessibilityNodeInfo()
      assertTrue(info.actionList.any { it.label == "Inspect" })
      assertTrue(info.actionList.any { it.label == "Share" })
      assertFalse(info.actionList.any { it.label == "Archive" })
      assertEquals("com.example.Reservation", info.className)
      info.recycle()
    }
    assertFalse(owner.performAction(archiveId))
    owner.recycle()
  }

  @Test fun native_menu_hold_owns_release_without_invoking_child_press() = attached { view, button ->
    val shown = CountDownLatch(1)
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    var presses = 0
    var down = 0L
    instrumentation.runOnMainSync {
      button.setOnClickListener { presses++ }
      view.contextMenu = { menu -> menu.add("Inspect"); true }
      view.onContextMenuWillShow = { shown.countDown() }
      down = SystemClock.uptimeMillis()
      touch(view, MotionEvent.ACTION_DOWN, down, 100f, 50f)
    }
    assertTrue("The actual native menu must receive the hold", shown.await(3, TimeUnit.SECONDS))
    instrumentation.runOnMainSync {
      touch(view, MotionEvent.ACTION_UP, down, 100f, 50f)
      assertEquals(0, presses)
    }
  }

  @Test fun menu_provider_detach_never_shows_an_orphan_or_changes_content_ownership() = attached { view, button ->
    val configured = CountDownLatch(1)
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    var willShow = 0
    instrumentation.runOnMainSync {
      val parent = view.parent as FrameLayout
      view.contextMenu = { menu ->
        menu.add("Inspect")
        parent.removeView(view)
        configured.countDown()
        true
      }
      view.onContextMenuWillShow = { willShow++ }
      val down = SystemClock.uptimeMillis()
      touch(view, MotionEvent.ACTION_DOWN, down, 100f, 50f)
    }
    assertTrue(configured.await(3, TimeUnit.SECONDS))
    instrumentation.runOnMainSync {
      assertEquals(0, willShow)
      assertFalse(view.isAttachedToWindow)
      assertSame(view.contentView, button.parent)
      assertEquals(0f, view.cell.translationX)
    }
  }

  @Test fun replaced_menu_provider_does_not_present_its_stale_configuration() = attached { view, _ ->
    val configured = CountDownLatch(1)
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    var willShow = 0
    instrumentation.runOnMainSync {
      view.contextMenu = { menu ->
        menu.add("Stale reservation menu")
        view.contextMenu = null
        configured.countDown()
        true
      }
      view.onContextMenuWillShow = { willShow++ }
      touch(view, MotionEvent.ACTION_DOWN, SystemClock.uptimeMillis(), 100f, 50f)
    }
    assertTrue(configured.await(3, TimeUnit.SECONDS))
    instrumentation.runOnMainSync { assertEquals(0, willShow); assertNull(view.contextMenu) }
  }

  @Test fun will_show_cancellation_still_owns_the_primary_release() = attached { view, button ->
    val attempted = CountDownLatch(1)
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    var presses = 0
    var down = 0L
    instrumentation.runOnMainSync {
      button.setOnClickListener { presses++ }
      view.contextMenu = { menu -> menu.add("Inspect"); true }
      view.onContextMenuWillShow = { view.contextMenu = null; attempted.countDown() }
      down = SystemClock.uptimeMillis()
      touch(view, MotionEvent.ACTION_DOWN, down, 100f, 50f)
    }
    assertTrue(attempted.await(3, TimeUnit.SECONDS))
    instrumentation.runOnMainSync {
      touch(view, MotionEvent.ACTION_UP, down, 100f, 50f)
      assertEquals(0, presses)
      assertNull(view.contextMenu)
    }
  }

  @Test fun hidden_subtree_never_donates_actions_to_an_outer_semantic_group() = attached { view, _ ->
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    lateinit var group: FrameLayout
    lateinit var hidden: FrameLayout
    lateinit var configuration: ShadowListKitSwipeActionsConfiguration
    var performed = 0
    instrumentation.runOnMainSync {
      val parent = view.parent as FrameLayout
      parent.removeView(view)
      group = FrameLayout(view.context).apply {
        contentDescription = "Reservations"
        importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
        ViewCompat.setScreenReaderFocusable(this, true)
      }
      hidden = FrameLayout(view.context).apply { importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS }
      parent.addView(group, FrameLayout.LayoutParams(300, 100))
      group.addView(hidden, FrameLayout.LayoutParams(-1, -1))
      hidden.addView(view, FrameLayout.LayoutParams(-1, -1))
      ViewCompat.addAccessibilityAction(group, "Inspect group") { _, _ -> true }
      configuration = ShadowListKitSwipeActionsConfiguration(listOf(
        ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.NORMAL, "Archive") { _, done -> performed++; done(true) }))
      view.trailingSwipeActions = configuration
    }
    instrumentation.waitForIdleSync(); instrumentation.uiAutomation.waitForIdle(100, 3000)
    val root = requireNotNull(instrumentation.uiAutomation.rootInActiveWindow)
    val groups = root.findAccessibilityNodeInfosByText("Reservations")
    val exposed = requireNotNull(groups.firstOrNull { it.contentDescription == "Reservations" })
    assertTrue(exposed.actionList.any { it.label == "Inspect group" })
    assertFalse(exposed.actionList.any { it.label == "Archive" })
    groups.forEach { it.recycle() }; root.recycle()
    instrumentation.runOnMainSync {
      hidden.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
      view.trailingSwipeActions = configuration
      val shown = group.createAccessibilityNodeInfo()
      assertEquals(1, shown.actionList.count { it.label == "Archive" })
      val archiveId = shown.actionList.first { it.label == "Archive" }.id
      shown.recycle()
      hidden.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
      assertFalse(group.performAccessibilityAction(archiveId, null))
      assertEquals(0, performed)
      view.requestLayout()
      view.measure(View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY))
      view.layout(0, 0, 300, 100)
      val removed = group.createAccessibilityNodeInfo()
      assertFalse(removed.actionList.any { it.label == "Archive" })
      assertTrue(removed.actionList.any { it.label == "Inspect group" })
      removed.recycle()
      hidden.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
      view.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
      view.trailingSwipeActions = configuration
      val ownHidden = group.createAccessibilityNodeInfo()
      assertFalse(ownHidden.actionList.any { it.label == "Archive" })
      ownHidden.recycle()
    }
  }
}
