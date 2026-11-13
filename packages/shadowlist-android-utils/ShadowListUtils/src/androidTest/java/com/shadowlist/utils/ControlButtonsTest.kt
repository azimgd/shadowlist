package com.shadowlist.utils

import android.content.Intent
import android.graphics.Rect
import android.graphics.drawable.RippleDrawable
import android.os.Build
import android.os.SystemClock
import android.view.MotionEvent
import android.view.TouchDelegate
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Test

class ControlButtonsTest {
  @Test fun pill_variants_and_disabled_action_are_controlled() = onMain {
    val button = ShadowListUtilsPillButton(testContext, "Continue", testTheme)
    var presses = 0
    button.onPress = { presses++ }
    assertEquals(ShadowListUtilsPillButtonVariant.PRIMARY, button.variant)
    assertEquals(testTheme.typography.body.letterSpacing / testTheme.typography.body.fontSize, button.letterSpacing, 0.0001f)
    button.performClick(); assertEquals(1, presses)
    button.disabled = true; button.performClick()
    assertEquals(1, presses); assertEquals(0.4f, button.alpha)
    button.disabled = false; button.variant = ShadowListUtilsPillButtonVariant.DESTRUCTIVE
    assertEquals(testTheme.colors.red, button.currentTextColor)
    assertEquals("Continue", button.contentDescription)
    button.measure(View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    assertEquals(40f.px, button.measuredHeight); assertTrue(button.measuredWidth >= 72f.px)
  }
  @Test fun ripple_is_native_and_other_buttons_keep_their_highlight_behavior() = onMain {
    val theme = testTheme.copy(pressFeedback = ShadowListUtilsThemePressFeedback.RIPPLE)
    val pill = ShadowListUtilsPillButton(testContext, "Continue", theme)
    assertTrue(pill.background is RippleDrawable)
    pill.isPressed = true; assertEquals(1f, pill.alpha)
    val highlighted = ShadowListUtilsPillButton(testContext, "Continue", testTheme)
    highlighted.isPressed = true; assertEquals(0.55f, highlighted.alpha)
    val form = ShadowListUtilsFormButton(testContext, "Save", theme)
    form.isPressed = true; assertEquals(0.55f, form.alpha)
  }
  @Test fun dashed_button_grows_for_large_text_and_disables_its_callback() = onMain {
    val config = android.content.res.Configuration(testContext.resources.configuration).apply { fontScale = 1.5f }
    val context = testContext.createConfigurationContext(config)
    val button = ShadowListUtilsDashedButton(context, "Add another route with a long descriptive label", testTheme)
    button.measure(View.MeasureSpec.makeMeasureSpec(150f.px, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    assertTrue(button.measuredHeight > testTheme.tapTarget.px)
    var presses = 0; button.onPress = { presses++ }; button.disabled = true; button.performClick()
    assertEquals(0, presses); assertEquals(0.4f, button.alpha)
  }
  @Test fun icon_and_badge_keep_native_accessible_identity_and_fixed_sizes() = onMain {
    val icon = View(testContext).apply { layoutParams = ViewGroup.LayoutParams(18f.px, 18f.px); importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO }
    val button = ShadowListUtilsIconButton(testContext, icon, "More routes", testTheme)
    assertEquals("More routes", button.contentDescription)
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, icon.importantForAccessibility)
    val info = android.view.accessibility.AccessibilityNodeInfo.obtain()
    button.onInitializeAccessibilityNodeInfo(info)
    assertEquals(android.widget.Button::class.java.name, info.className)
    info.recycle()
    val replacement = android.widget.Button(testContext).apply { contentDescription = "Nested action"; importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES }
    var nestedPresses = 0
    replacement.setOnClickListener { nestedPresses++ }
    button.icon = replacement
    assertNull(icon.parent); assertSame(button, replacement.parent)
    assertTrue(replacement.isClickable); assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_YES, replacement.importantForAccessibility)
    replacement.performClick(); assertEquals(1, nestedPresses)
    val badge = ShadowListUtilsIndexBadge(testContext, 0, testTheme)
    badge.measure(0, 0)
    assertEquals(22f.px, badge.measuredWidth); assertEquals(22f.px, badge.measuredHeight)
    assertEquals("0", badge.text.toString()); badge.index = 3
    assertEquals("3", badge.text.toString())
  }

  @Test fun expanded_siblings_preserve_other_delegates_coordinates_and_lifecycle() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val automation = instrumentation.uiAutomation
    val activity = instrumentation.startActivitySync(Intent().setClassName(context.packageName,
      ShadowListUtilsAssistantTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    val focused = CountDownLatch(1)
    lateinit var parent: FrameLayout
    lateinit var secondParent: FrameLayout
    lateinit var first: ShadowListUtilsIconButton
    lateinit var second: ShadowListUtilsIconButton
    lateinit var original: TouchDelegate
    var firstPresses = 0; var secondPresses = 0; var nestedPresses = 0
    val originalCoordinates = mutableListOf<Pair<Float, Float>>()
    var originalConsumes = false
    fun tap(x: Float, y: Float): Boolean {
      var handled = false
      onMain {
        val now = SystemClock.uptimeMillis()
        for (action in listOf(MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP)) {
          val event = MotionEvent.obtain(now, now + action, action, x.pxf, y.pxf, 0)
          try {
            handled = parent.dispatchTouchEvent(event)
            assertEquals(x.pxf, event.x, 0.01f); assertEquals(y.pxf, event.y, 0.01f)
          } finally { event.recycle() }
        }
      }
      instrumentation.waitForIdleSync()
      return handled
    }
    try {
      onMain {
        parent = FrameLayout(activity)
        original = object : TouchDelegate(Rect(), parent) {
          override fun onTouchEvent(event: MotionEvent): Boolean {
            originalCoordinates.add(event.x to event.y)
            event.setLocation(-100f, -100f)
            return originalConsumes
          }
        }
        parent.touchDelegate = original
        val nested = android.widget.Button(activity).apply {
          layoutParams = ViewGroup.LayoutParams(24f.px, 24f.px)
          contentDescription = "Nested first"; importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
          setOnClickListener { nestedPresses++ }
        }
        first = ShadowListUtilsIconButton(activity, nested, "First", testTheme).apply { onPress = { firstPresses++ } }
        second = ShadowListUtilsIconButton(activity, View(activity), "Second", testTheme).apply { onPress = { secondPresses++ } }
        parent.addView(first, FrameLayout.LayoutParams(40f.px, 40f.px).apply { leftMargin = 20f.px; topMargin = 20f.px })
        parent.addView(second, FrameLayout.LayoutParams(40f.px, 40f.px).apply { leftMargin = 100f.px; topMargin = 20f.px })
        secondParent = FrameLayout(activity).apply {
          contentDescription = "Expanded controls"
          importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
        }
        parent.addView(secondParent, FrameLayout.LayoutParams(200f.px, 80f.px).apply { topMargin = 100f.px })
        activity.setContentView(parent)
        activity.window.decorView.viewTreeObserver.addOnWindowFocusChangeListener { if (it) focused.countDown() }
        if (activity.window.decorView.hasWindowFocus()) focused.countDown()
      }
      assertTrue(focused.await(5, TimeUnit.SECONDS))
      onMain {
        val info = first.createAccessibilityNodeInfo()
        val bounds = Rect()
        info.getBoundsInScreen(bounds)
        assertEquals(first.width, bounds.width()); assertEquals(first.height, bounds.height())
        assertEquals(android.widget.Button::class.java.name, info.className)
        info.recycle()
      }
      if (Build.VERSION.SDK_INT >= 29) onMain {
        assertSame(original.touchDelegateInfo, parent.touchDelegate.touchDelegateInfo)
      }
      tap(18f, 40f); tap(98f, 40f)
      onMain { assertEquals(1, firstPresses); assertEquals(1, secondPresses); assertEquals(0, nestedPresses) }
      tap(40f, 40f)
      onMain { assertEquals(1, nestedPresses); assertEquals(1, firstPresses); first.isEnabled = false }
      tap(18f, 40f)
      onMain {
        assertEquals(1, firstPresses)
        assertEquals(18f.pxf to 40f.pxf, originalCoordinates.last())
        first.isEnabled = true; originalConsumes = true
      }
      assertTrue(tap(190f, 190f))
      onMain {
        originalConsumes = false
        second.layoutParams = (second.layoutParams as FrameLayout.LayoutParams).apply { leftMargin = 140f.px }
        parent.measure(View.MeasureSpec.makeMeasureSpec(parent.width, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(parent.height, View.MeasureSpec.EXACTLY))
        parent.layout(parent.left, parent.top, parent.right, parent.bottom)
      }
      tap(98f, 40f); tap(138f, 40f)
      onMain {
        assertEquals(2, secondPresses)
        parent.removeView(first)
        secondParent.addView(first, FrameLayout.LayoutParams(40f.px, 40f.px).apply { leftMargin = 20f.px; topMargin = 20f.px })
        parent.measure(View.MeasureSpec.makeMeasureSpec(parent.width, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(parent.height, View.MeasureSpec.EXACTLY))
        parent.layout(parent.left, parent.top, parent.right, parent.bottom)
      }
      tap(18f, 40f); tap(18f, 140f)
      if (Build.VERSION.SDK_INT >= 29) {
        onMain {
          val now = SystemClock.uptimeMillis()
          for (action in listOf(MotionEvent.ACTION_HOVER_ENTER, MotionEvent.ACTION_HOVER_EXIT)) {
            val event = MotionEvent.obtain(now, now, action, 18f.pxf, 40f.pxf, 0)
            try {
              assertTrue(secondParent.touchDelegate.onTouchExplorationHoverEvent(event))
              assertEquals(action == MotionEvent.ACTION_HOVER_ENTER, first.isHovered)
              assertEquals(18f.pxf, event.x, 0.01f); assertEquals(40f.pxf, event.y, 0.01f)
            } finally { event.recycle() }
          }
        }
        instrumentation.waitForIdleSync()
        val root = requireNotNull(automation.rootInActiveWindow)
        val container = root.findAccessibilityNodeInfosByText("Expanded controls").first()
        val delegates = requireNotNull(container.touchDelegateInfo)
        assertEquals(1, delegates.regionCount)
        val target = requireNotNull(delegates.getTargetForRegion(delegates.getRegionAt(0)))
        assertEquals("First", target.contentDescription)
        assertEquals(android.widget.Button::class.java.name, target.className)
        target.recycle(); container.recycle(); root.recycle()
      }
      onMain {
        assertEquals(2, firstPresses)
        parent.removeView(second); assertSame(original, parent.touchDelegate)
        secondParent.removeView(first); assertNull(secondParent.touchDelegate)
      }
    } finally { onMain { activity.finish() } }
  }
}
