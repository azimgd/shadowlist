package com.shadowlist.utils

import android.content.Intent
import android.content.res.Configuration
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Test

class SegmentedTest {
  private val options = listOf(ShadowListUtilsSegmentedOption("plan", "Plan"), ShadowListUtilsSegmentedOption("book", "Book"), ShadowListUtilsSegmentedOption("travel", "Travel"))
  private fun buttons(view: ViewGroup): List<ShadowListUtilsSegmentedButton> = (0 until view.childCount).flatMap { index ->
    val child = view.getChildAt(index)
    if (child is ShadowListUtilsSegmentedButton) listOf(child) else if (child is ViewGroup) buttons(child) else emptyList()
  }
  private fun place(view: View) {
    view.measure(View.MeasureSpec.makeMeasureSpec(240f.px, View.MeasureSpec.EXACTLY), 0)
    view.layout(0, 0, view.measuredWidth, view.measuredHeight)
  }
  @Test fun selection_is_controlled_and_selected_taps_still_notify() = onMain {
    val control = ShadowListUtilsSegmented(testContext, options, "plan", testTheme)
    val changes = mutableListOf<String>(); control.onChange = { changes.add(it) }
    val cells = buttons(control)
    cells[0].performClick(); cells[0].performClick(); cells[1].performClick()
    assertEquals(listOf("plan", "plan", "book"), changes); assertEquals("plan", control.value)
    control.value = "book"; assertEquals(3, changes.size)
    assertFalse(cells[0].isSelected); assertTrue(cells[1].isSelected)
    control.value = "missing"; assertTrue(cells.all { !it.isSelected })
    control.options = emptyList(); place(control)
    assertTrue(buttons(control).isEmpty()); assertEquals(36f.px, control.measuredHeight)
  }
  @Test fun defaults_dividers_highlight_and_compact_metrics_match() = onMain {
    val control = ShadowListUtilsSegmented(testContext, options, "plan", testTheme)
    place(control)
    val cells = buttons(control)
    assertEquals(ShadowListUtilsSegmentedSize.REGULAR, control.size); assertEquals(36f.px, control.height)
    assertTrue(kotlin.math.abs(cells[0].width - cells[1].width) <= 1)
    assertEquals(View.GONE, cells[0].divider.visibility); assertEquals(View.GONE, cells[1].divider.visibility); assertEquals(View.VISIBLE, cells[2].divider.visibility)
    cells[0].isPressed = true; assertEquals(1f, cells[0].alpha)
    cells[2].isPressed = true; assertEquals(0.55f, cells[2].alpha)
    control.size = ShadowListUtilsSegmentedSize.COMPACT; place(control)
    assertEquals(34f.px, control.height); assertTrue((control.getChildAt(0)).width < control.width)
    assertEquals(testTheme.colors.accent, cells[0].label.currentTextColor)
  }
  @Test fun RTL_and_large_text_use_native_layout_without_shrinking_labels() = onMain {
    val context = testContext.createConfigurationContext(Configuration(testContext.resources.configuration).apply { fontScale = 3f })
    val control = ShadowListUtilsSegmented(context, options, "plan", testTheme)
    control.layoutDirection = View.LAYOUT_DIRECTION_RTL; place(control)
    val cells = buttons(control)
    assertTrue(cells[0].left > cells[2].left)
    assertTrue(cells[0].label.textSize > 13f.pxf)
    assertTrue(cells[0].label.height > cells[0].height)
    assertEquals("Plan", cells[0].contentDescription); assertFalse(cells[0].clipChildren)
  }
  @Test fun stable_ids_keep_native_focus_and_keyboard_activation_returns_the_current_id() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val activity = instrumentation.startActivitySync(Intent().setClassName(instrumentation.context.packageName,
      ShadowListUtilsAssistantTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    val focused = CountDownLatch(1)
    lateinit var control: ShadowListUtilsSegmented
    lateinit var first: ShadowListUtilsSegmentedButton
    val changes = mutableListOf<String>()
    try {
      onMain {
        control = ShadowListUtilsSegmented(activity, options, "plan", testTheme)
        control.onChange = { changes.add(it) }; control.accessibilityLabel = "Trip stage"
        activity.setContentView(control)
        first = buttons(control)[0]
        activity.window.decorView.viewTreeObserver.addOnWindowFocusChangeListener { if (it) focused.countDown() }
        if (activity.window.decorView.hasWindowFocus()) focused.countDown()
      }
      assertTrue(focused.await(5, TimeUnit.SECONDS))
      onMain {
        control.options = listOf(options[2], ShadowListUtilsSegmentedOption("plan", "Planning"))
        assertSame(first, buttons(control)[1]); assertEquals("Planning", first.contentDescription)
        first.isFocusableInTouchMode = true; assertTrue(first.requestFocus())
        val info = first.createAccessibilityNodeInfo()
        assertEquals("android.app.ActionBar\$Tab", info.className); assertTrue(info.isSelected)
        assertEquals(1, info.collectionItemInfo.columnIndex); info.recycle()
        val group = control.createAccessibilityNodeInfo()
        assertEquals("Trip stage", group.contentDescription); assertEquals(2, group.collectionInfo.columnCount); group.recycle()
        first.dispatchKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER))
        first.dispatchKeyEvent(KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_CENTER))
      }
      instrumentation.waitForIdleSync()
      onMain { assertEquals(listOf("plan"), changes); assertEquals("plan", control.value) }
    } finally { onMain { activity.finish() } }
  }
}
