package com.shadowlist.utils

import android.content.res.Configuration
import android.content.Intent
import android.graphics.Color
import android.graphics.RectF
import android.util.SizeF
import android.view.View
import android.view.MotionEvent
import android.widget.Button
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Test

class ChipTest {
  private fun content(chip: android.view.ViewGroup) = chip.getChildAt(0) as ShadowListUtilsChipContent
  private fun place(chip: View) {
    chip.measure(View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    chip.layout(0, 0, chip.measuredWidth, chip.measuredHeight)
  }

  @Test fun optional_press_keeps_normal_appearance_and_clear_is_independent() = onMain {
    val chip = ShadowListUtilsChip(testContext, "Lisbon", testTheme)
    var clears = 0; var presses = 0
    chip.onClear = { clears++ }
    val view = content(chip)
    assertTrue(view.button.isEnabled)
    view.button.isPressed = true; assertEquals(1f, view.alpha)
    view.clearButton.performClick()
    assertEquals(1, clears); assertEquals(0, presses); assertFalse(chip.isSelected)
    assertEquals("Clear Lisbon", view.clearButton.contentDescription)
    chip.onPress = { presses++ }
    view.button.isPressed = true; assertEquals(0.55f, view.alpha)
    view.button.performClick()
    assertEquals(1, presses); assertEquals(1, clears); assertFalse(chip.isSelected)
    chip.isSelected = true; chip.variant = ShadowListUtilsChipVariant.PLAIN
    assertEquals(1, presses); assertEquals(1, clears)
    assertTrue(view.button.isSelected); assertFalse(view.clearButton.isSelected)
    chip.accessibilityLabel = "Destination"; chip.clearAccessibilityLabel = "Remove destination"
    assertEquals("Destination", view.button.contentDescription)
    assertEquals("Remove destination", view.clearButton.contentDescription)
    chip.onClear = null; assertEquals(View.GONE, view.clearButton.visibility)
  }

  @Test fun font_cap_mono_compact_and_maximum_width_match_the_label_contract() = onMain {
    val context = testContext.createConfigurationContext(Configuration(testContext.resources.configuration).apply { fontScale = 2f })
    val chip = ShadowListUtilsChip(context, "Destination ".repeat(12), testTheme)
    chip.onClear = {}; chip.maxFontSizeMultiplier = 1f; chip.mono = true
    val view = content(chip)
    assertEquals(15f * context.resources.displayMetrics.density, view.button.textSize, 0.1f)
    assertEquals(-0.6f / 15f, view.button.letterSpacing, 0.001f)
    assertTrue(view.clearButton.textSize > view.button.textSize)
    place(chip); assertEquals(240f.px, chip.measuredWidth); assertEquals(34f.px, chip.measuredHeight)
    chip.size = ShadowListUtilsChipSize.COMPACT; place(chip); assertEquals(28f.px, chip.measuredHeight)
    chip.maxFontSizeMultiplier = 0f
    assertTrue(view.button.textSize > 13f * context.resources.displayMetrics.density)
  }

  @Test fun choice_selection_and_tint_remain_controlled() = onMain {
    val chip = ShadowListUtilsChoiceChip(testContext, "Train", Color.YELLOW, false, testTheme)
    var presses = 0; chip.onPress = { presses++ }
    val view = content(chip)
    view.button.performClick()
    assertEquals(1, presses); assertFalse(chip.isSelected); assertNull(view.leadingView)
    chip.isSelected = true
    assertEquals(1, presses); assertNotNull(view.leadingView)
    assertEquals(2f, view.borderWidth); assertEquals(Color.YELLOW, view.fillOverride)
    assertTrue(view.button.isSelected); assertTrue(view.button.isEnabled)
  }

  @Test fun shared_wrap_bridge_preserves_exact_fit_and_variable_line_height() = onMain {
    val layout = ShadowListUtilsWrapLayout(100f, listOf(SizeF(40f, 20f), SizeF(52f, 34f), SizeF(30f, 28f)), 8f)
    assertEquals(listOf(RectF(0f, 0f, 40f, 20f), RectF(48f, 0f, 100f, 34f), RectF(0f, 42f, 30f, 70f)), layout.frames)
    assertEquals(70f, layout.height)
  }

  @Test fun row_mirrors_shared_frames_and_keeps_child_identity_across_updates() = onMain {
    val previous = ShadowListUtilsPlatform.isRightToLeftOverride
    ShadowListUtilsPlatform.isRightToLeftOverride = false
    try {
      val first = View(testContext).apply { minimumWidth = 40f.px; minimumHeight = 20f.px }
      val second = View(testContext).apply { minimumWidth = 52f.px; minimumHeight = 34f.px }
      val third = View(testContext).apply { minimumWidth = 30f.px; minimumHeight = 28f.px }
      val row = ShadowListUtilsChipRow(testContext, listOf(first, second, third), testTheme)
      val width = View.MeasureSpec.makeMeasureSpec(100f.px, View.MeasureSpec.EXACTLY)
      row.measure(width, 0); row.layout(0, 0, row.measuredWidth, row.measuredHeight)
      assertEquals(42f.px, third.top); assertEquals(70f.px, row.measuredHeight)
      ShadowListUtilsPlatform.isRightToLeftOverride = true
      row.requestLayout(); row.measure(width, 0); row.layout(0, 0, row.measuredWidth, row.measuredHeight)
      assertEquals(60f.px, first.left); assertEquals(0, second.left); assertEquals(70f.px, third.left)
      row.children = listOf(third, first); row.measure(width, 0); row.layout(0, 0, row.measuredWidth, row.measuredHeight)
      assertSame(row, third.parent); assertNull(second.parent)
      row.children = emptyList(); row.measure(width, 0)
      assertNull(third.parent); assertEquals(0, row.measuredHeight)
    } finally { ShadowListUtilsPlatform.isRightToLeftOverride = previous }
  }

  @Test fun attached_clear_expansion_does_not_invoke_the_main_action() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val activity = instrumentation.startActivitySync(Intent().setClassName(instrumentation.context.packageName,
      ShadowListUtilsAssistantTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    val focused = CountDownLatch(1)
    lateinit var parent: FrameLayout
    lateinit var chip: ShadowListUtilsChip
    lateinit var choice: ShadowListUtilsChoiceChip
    var clears = 0; var presses = 0
    try {
      onMain {
        parent = FrameLayout(activity)
        chip = ShadowListUtilsChip(activity, "Lisbon", testTheme).apply {
          onPress = { presses++ }; onClear = { clears++ }
        }
        parent.addView(chip, FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT).apply {
          leftMargin = 20f.px; topMargin = 20f.px
        })
        choice = ShadowListUtilsChoiceChip(activity, "Train", Color.YELLOW, true, testTheme).apply { onPress = {} }
        parent.addView(choice, FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT).apply {
          leftMargin = 20f.px; topMargin = 80f.px
        })
        activity.setContentView(parent)
        activity.window.decorView.viewTreeObserver.addOnWindowFocusChangeListener { if (it) focused.countDown() }
        if (activity.window.decorView.hasWindowFocus()) focused.countDown()
      }
      assertTrue(focused.await(5, TimeUnit.SECONDS))
      onMain {
        val info = content(choice).button.createAccessibilityNodeInfo()
        assertEquals(Button::class.java.name, info.className); assertTrue(info.isSelected); info.recycle()
        val clear = content(chip).clearButton
        val x = chip.left + clear.left - 6f.pxf
        val y = chip.top + chip.height / 2f
        val now = android.os.SystemClock.uptimeMillis()
        for (action in listOf(MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP)) {
          val event = MotionEvent.obtain(now, now + action, action, x, y, 0)
          try { assertTrue(parent.dispatchTouchEvent(event)) } finally { event.recycle() }
        }
      }
      instrumentation.waitForIdleSync()
      onMain { assertEquals(1, clears); assertEquals(0, presses); assertFalse(chip.isSelected) }
    } finally { onMain { activity.finish() } }
  }
}
