package com.shadowlist.utils

import android.view.View
import android.widget.LinearLayout
import org.junit.Assert.*
import org.junit.Test

class FormGroupedCompositionTest {
  @Test fun form_card_separates_rows_and_preserves_child_identity() = onMain {
    val first = ShadowListUtilsFormButton(testContext, "Save", testTheme)
    val second = ShadowListUtilsFormButton(testContext, "Delete", testTheme)
    val card = ShadowListUtilsFormCard(testContext, listOf(first, second), testTheme)
    assertEquals(3, card.childCount)
    val separator = card.getChildAt(1)
    card.children = listOf(first, second)
    assertSame(separator, card.getChildAt(1))
    card.children = listOf(second)
    assertEquals(1, card.childCount); assertSame(second, card.getChildAt(0)); assertNull(first.parent)
  }
  @Test fun form_value_and_button_expose_application_state_and_actions() = onMain {
    val value = ShadowListUtilsFormValue(testContext, "", testTheme)
    assertEquals("", value.text); assertEquals("—", value.contentDescription)
    value.text = "€24"; value.accessibilityLabel = "Twenty four euros"
    assertEquals("Twenty four euros", value.contentDescription)
    val button = ShadowListUtilsFormButton(testContext, "Delete", testTheme)
    assertFalse(button.isEnabled)
    var pressed = 0
    button.onPress = { pressed++ }; button.destructive = true; button.performClick()
    assertEquals(1, pressed); assertEquals(testTheme.colors.red, button.currentTextColor)
    assertEquals("Delete", button.contentDescription)
    assertEquals(android.view.Gravity.CENTER, button.gravity)
    button.isPressed = true; assertEquals(0.55f, button.alpha)
    button.isPressed = false; assertEquals(1f, button.alpha)
  }
  @Test fun grouped_composition_keeps_explicit_separators_and_optional_caption() = onMain {
    val first = ShadowListUtilsFormValue(testContext, "Account", testTheme)
    val separator = ShadowListUtilsGroupedSeparator(testContext, 24f, testTheme)
    val card = ShadowListUtilsGroupedCard(testContext, listOf(first, separator), testTheme)
    val stack = card.getChildAt(0) as LinearLayout
    assertEquals(2, stack.childCount)
    card.children = listOf(first, separator); assertSame(first, stack.getChildAt(0))
    val section = ShadowListUtilsGroupedSection(testContext, children = listOf(card), theme = testTheme)
    val caption = section.getChildAt(0) as ShadowListUtilsGroupedCaption
    assertEquals(View.GONE, caption.visibility)
    section.title = "Account"; assertEquals(View.VISIBLE, caption.visibility)
    assertTrue(caption.isAccessibilityHeading)
    section.title = ""; assertEquals(View.GONE, caption.visibility)
  }
  @Test fun separator_is_one_physical_pixel_and_decorative_in_both_directions() = onMain {
    val line = ShadowListUtilsGroupedSeparator(testContext, 30f, testTheme)
    line.measure(View.MeasureSpec.makeMeasureSpec(200f.px, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    assertEquals(1, line.measuredHeight)
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, line.importantForAccessibility)
    line.layoutDirection = View.LAYOUT_DIRECTION_RTL
    assertEquals(30f, line.inset)
  }
}
