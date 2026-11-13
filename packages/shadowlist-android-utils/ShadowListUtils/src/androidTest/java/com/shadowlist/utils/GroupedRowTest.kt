package com.shadowlist.utils

import android.content.Intent
import android.content.res.Configuration
import android.graphics.Color
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.TextView
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat
import androidx.test.platform.app.InstrumentationRegistry
import com.shadowlist.kit.ShadowListKitItemActionsView
import com.shadowlist.kit.ShadowListKitSwipeAction
import com.shadowlist.kit.ShadowListKitSwipeActionsConfiguration
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Test

class GroupedRowTest {
  private fun views(view: View): List<View> = listOf(view) +
    if (view is ViewGroup) (0 until view.childCount).flatMap { views(view.getChildAt(it)) } else emptyList()

  private fun place(view: View, width: Int = 220f.px): Int {
    view.measure(View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    view.layout(0, 0, view.measuredWidth, view.measuredHeight)
    return view.measuredHeight
  }

  @Test fun optional_press_keeps_normal_role_label_and_test_identifier() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val activity = instrumentation.startActivitySync(Intent().setClassName(instrumentation.context.packageName,
      ShadowListUtilsAssistantTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    try {
      onMain {
        val row = ShadowListUtilsGroupedRow(activity, "Reservation", testTheme)
        activity.setContentView(row)
        assertTrue(row.isAttachedToWindow)
        assertEquals(1f, row.alpha)
        var info = row.createAccessibilityNodeInfo()
        assertTrue(info.isEnabled); assertEquals("android.widget.Button", info.className)
        assertEquals("Reservation", info.contentDescription); info.recycle()
        assertTrue(row.performClick())
        row.subtitle = "Kyoto"; row.testID = "reservation"
        row.accessibilityRole = ShadowListUtilsGroupedRowAccessibilityRole.LINK
        info = row.createAccessibilityNodeInfo()
        assertEquals("Reservation, Kyoto", info.contentDescription)
        assertEquals("reservation", info.viewIdResourceName)
        assertEquals("Link", AccessibilityNodeInfoCompat.wrap(info).roleDescription)
        info.recycle()
      }
    } finally { onMain { activity.finish() } }
  }

  @Test fun native_slots_and_large_text_keep_child_state_and_grow() = onMain {
    val row = ShadowListUtilsGroupedRow(testContext, "A long reservation title that wraps at a narrow width", testTheme)
    row.subtitle = "An address with arrival information and reservation instructions"
    val leading = Button(testContext).apply {
      text = "Map"; translationX = 3f; alpha = 0.8f; contentDescription = "Open map"
    }
    val trailing = Button(testContext).apply { text = "More" }
    row.leading = leading; row.trailing = trailing; row.separated = true
    val normal = place(row)
    assertTrue(normal >= testTheme.grouped.rowHeight.px)
    val old = ShadowListUtilsPlatform.textScaleOverride
    ShadowListUtilsPlatform.textScaleOverride = 2f
    try {
      row.dispatchConfigurationChanged(Configuration(testContext.resources.configuration))
      assertTrue(place(row) > normal)
      assertTrue(views(row).filterIsInstance<TextView>().first { it.text.startsWith("A long reservation") }.maxLines == 3)
      assertTrue(leading.isClickable); assertEquals("Open map", leading.contentDescription)
      assertEquals(3f, leading.translationX); assertEquals(0.8f, leading.alpha)
      assertTrue(trailing.isClickable)
    } finally { ShadowListUtilsPlatform.textScaleOverride = old }
  }

  @Test fun destructive_theming_copies_originals_and_retains_explicit_colors() = onMain {
    val implicit = ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.DESTRUCTIVE, "Remove") { _, _ -> }
    val explicit = ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.DESTRUCTIVE, "Explicit") { _, _ -> }.apply { backgroundColor = Color.RED }
    val clear = ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.DESTRUCTIVE, "Clear") { _, _ -> }.apply { backgroundColor = Color.TRANSPARENT }
    val configuration = ShadowListKitSwipeActionsConfiguration(listOf(implicit, explicit, clear)).apply { performsFirstActionWithFullSwipe = false }
    val wrapper = ShadowListUtilsGroupedActions(testContext, listOf(View(testContext)), testTheme)
    wrapper.trailingSwipeActions = configuration
    val native = views(wrapper).filterIsInstance<ShadowListKitItemActionsView>().single()
    wrapper.theme = testTheme.copy(colors = testTheme.colors.copy(red = Color.MAGENTA))
    val themed = requireNotNull(native.trailingSwipeActions)
    assertEquals(Color.MAGENTA, themed.actions[0].backgroundColor)
    assertSame(explicit, themed.actions[1]); assertSame(clear, themed.actions[2])
    assertNull(implicit.backgroundColor); assertFalse(themed.performsFirstActionWithFullSwipe)
    wrapper.theme = testTheme.copy(colors = testTheme.colors.copy(red = Color.YELLOW))
    assertEquals(Color.YELLOW, requireNotNull(native.trailingSwipeActions).actions[0].backgroundColor)
    assertEquals(3, configuration.actions.size)
  }

  @Test fun disabled_exposed_row_keeps_labelled_accessibility_and_swipe_actions_independent() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val automation = instrumentation.uiAutomation
    val activity = instrumentation.startActivitySync(Intent().setClassName(instrumentation.context.packageName,
      ShadowListUtilsAssistantTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    val focused = CountDownLatch(1)
    var presses = 0; var holds = 0; var archives = 0
    lateinit var row: ShadowListUtilsGroupedRow
    try {
      onMain {
        row = ShadowListUtilsGroupedRow(activity, "Reservation", testTheme)
        row.onPress = { presses++ }; row.onLongPress = { holds++ }; row.longPressLabel = "Details"
        row.trailingSwipeActions = ShadowListKitSwipeActionsConfiguration(listOf(
          ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.NORMAL, "Archive") { _, done -> archives++; done(true) }))
        row.disabled = true
        val root = FrameLayout(activity); root.addView(row, FrameLayout.LayoutParams(300, 100)); activity.setContentView(root)
        activity.window.decorView.viewTreeObserver.addOnWindowFocusChangeListener { if (it) focused.countDown() }
        if (activity.window.decorView.hasWindowFocus()) focused.countDown()
      }
      assertTrue(focused.await(5, TimeUnit.SECONDS))
      instrumentation.waitForIdleSync(); automation.waitForIdle(100, 3000)
      val root = requireNotNull(automation.rootInActiveWindow)
      val nodes = root.findAccessibilityNodeInfosByText("Reservation")
      val exposed = requireNotNull(nodes.firstOrNull { it.contentDescription == "Reservation" && it.className == "android.widget.Button" })
      assertFalse(exposed.isEnabled)
      val details = exposed.actionList.first { it.label == "Details" }
      val archive = exposed.actionList.first { it.label == "Archive" }
      assertEquals(1, exposed.actionList.count { it.label == "Archive" })
      assertTrue(exposed.performAction(details.id))
      assertTrue(exposed.performAction(archive.id))
      onMain {
        assertEquals(1, holds); assertEquals(1, archives)
        val down = android.os.SystemClock.uptimeMillis()
        for (action in listOf(MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP)) {
          val event = MotionEvent.obtain(down, android.os.SystemClock.uptimeMillis(), action, 150f, 30f, 0)
          try { row.dispatchTouchEvent(event) } finally { event.recycle() }
        }
        assertEquals(0, presses)
      }
      nodes.forEach { it.recycle() }; root.recycle()
    } finally { onMain { activity.finish() } }
  }
}
