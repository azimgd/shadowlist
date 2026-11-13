package com.shadowlist.utils

import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Test

class ProgressControlsTest {
  private fun drawn(view: View): Bitmap {
    val bitmap = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
    view.draw(Canvas(bitmap)); return bitmap
  }
  private fun pixel(color: Int): Int {
    val bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888)
    Canvas(bitmap).drawColor(color)
    val pixel = bitmap.getPixel(0, 0); bitmap.recycle(); return pixel
  }
  @Test fun helpers_replay_the_actual_RN_progress_vectors() = onMain {
    for (entry in Vectors.cases("progress.json")) {
      val input = entry.getJSONObject("input")
      val expected = entry.getJSONObject("output")
      val fraction = if (input.isNull("fraction")) Double.NaN else input.getDouble("fraction")
      assertEquals(expected.getDouble("fraction"), ShadowListUtilsCore.clampFraction(fraction), 0.0)
      assertEquals(expected.getInt("percent"), ShadowListUtilsCore.progressPercent(fraction))
    }
    assertEquals(0.0, ShadowListUtilsCore.clampFraction(Double.POSITIVE_INFINITY), 0.0)
    assertEquals(0.0, ShadowListUtilsCore.clampFraction(Double.NEGATIVE_INFINITY), 0.0)
  }
  @Test fun bar_defaults_paint_from_the_native_leading_edge() = onMain {
    val bar = ShadowListUtilsProgressBar(testContext, 0.5, "Trip preparation", theme = testTheme)
    bar.measure(View.MeasureSpec.makeMeasureSpec(200f.px, View.MeasureSpec.EXACTLY), 0)
    bar.layout(0, 0, bar.measuredWidth, bar.measuredHeight)
    assertEquals(6f.px, bar.height)
    var bitmap = drawn(bar)
    assertEquals(pixel(testTheme.colors.accent), bitmap.getPixel(25f.px, bar.height / 2))
    assertEquals(pixel(testTheme.colors.fill), bitmap.getPixel(150f.px, bar.height / 2)); bitmap.recycle()
    bar.layoutDirection = View.LAYOUT_DIRECTION_RTL
    bitmap = drawn(bar)
    assertEquals(pixel(testTheme.colors.fill), bitmap.getPixel(25f.px, bar.height / 2))
    assertEquals(pixel(testTheme.colors.accent), bitmap.getPixel(150f.px, bar.height / 2)); bitmap.recycle()
    bar.setHeight(8f); bar.measure(View.MeasureSpec.makeMeasureSpec(200f.px, View.MeasureSpec.EXACTLY), 0)
    assertEquals(8f.px, bar.measuredHeight)
    bar.fraction = Double.NaN; assertTrue(bar.fraction.isNaN())
    assertEquals("Trip preparation", bar.contentDescription)
  }
  @Test fun ring_is_decorative_and_fills_clockwise_even_in_RTL() = onMain {
    val ring = ShadowListUtilsProgressRing(testContext, 0.25, theme = testTheme)
    ring.layoutDirection = View.LAYOUT_DIRECTION_RTL
    ring.measure(0, 0); ring.layout(0, 0, ring.measuredWidth, ring.measuredHeight)
    assertEquals(52f, ring.size); assertEquals(7f, ring.strokeWidth)
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS, ring.importantForAccessibility)
    var bitmap = drawn(ring)
    assertEquals(pixel(testTheme.colors.accent), bitmap.getPixel(26f.px, 3f.px))
    assertEquals(pixel(testTheme.colors.accent), bitmap.getPixel(49f.px, 26f.px))
    assertEquals(pixel(testTheme.colors.accentSoft), bitmap.getPixel(26f.px, 49f.px))
    assertEquals(Color.TRANSPARENT, bitmap.getPixel(26f.px, 26f.px)); bitmap.recycle()
    ring.fraction = Double.POSITIVE_INFINITY
    bitmap = drawn(ring); assertEquals(pixel(testTheme.colors.accentSoft), bitmap.getPixel(26f.px, 3f.px)); bitmap.recycle()
  }
  @Test fun attached_value_updates_are_read_only_and_queue_no_animation() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val activity = instrumentation.startActivitySync(Intent().setClassName(instrumentation.context.packageName,
      ShadowListUtilsAssistantTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    try {
      onMain {
        val parent = FrameLayout(activity)
        val bar = ShadowListUtilsProgressBar(activity, 0.25, "Trip preparation", theme = testTheme)
        val ring = ShadowListUtilsProgressRing(activity, 0.25, theme = testTheme)
        parent.addView(bar, FrameLayout.LayoutParams(200f.px, 6f.px))
        parent.addView(ring, FrameLayout.LayoutParams(52f.px, 52f.px).apply { topMargin = 20f.px })
        activity.setContentView(parent)
        assertTrue(bar.isAttachedToWindow); assertTrue(ring.isAttachedToWindow)
        for (entry in Vectors.cases("progress.json")) {
          val input = entry.getJSONObject("input")
          bar.fraction = if (input.isNull("fraction")) Double.NaN else input.getDouble("fraction")
          val current = bar.createAccessibilityNodeInfo()
          assertEquals(entry.getJSONObject("output").getInt("percent").toFloat(), current.rangeInfo.current)
          current.recycle()
        }
        bar.fraction = 0.75; ring.fraction = 0.75
        val info = bar.createAccessibilityNodeInfo()
        assertEquals(android.widget.ProgressBar::class.java.name, info.className)
        assertEquals("Trip preparation", info.contentDescription)
        assertEquals(0f, info.rangeInfo.min); assertEquals(100f, info.rangeInfo.max); assertEquals(75f, info.rangeInfo.current)
        assertFalse(info.actionList.any { it.id == AccessibilityNodeInfo.AccessibilityAction.ACTION_SET_PROGRESS.id })
        info.recycle()
        assertNull(bar.animation); assertNull(ring.animation)
        assertFalse(bar.hasTransientState()); assertFalse(ring.hasTransientState())
      }
    } finally { onMain { activity.finish() } }
  }
}
