package com.shadowlist.utils

import org.junit.Assert.*
import org.junit.Test

class FormButtonAppearanceTest {
  @Test fun form_actions_remain_flat_in_normal_and_pressed_states() = onMain {
    val button = ShadowListUtilsFormButton(testContext, "Save route", testTheme)
    var presses = 0; button.onPress = { presses++ }
    assertNull(button.stateListAnimator)
    assertEquals(0f, button.elevation); assertEquals(0f, button.translationZ)
    button.isPressed = true
    assertEquals(0f, button.elevation); assertEquals(0f, button.translationZ)
    assertEquals(0.55f, button.alpha)
    button.performClick(); assertEquals(1, presses)
    button.isPressed = false
    assertEquals(1f, button.alpha)
    button.destructive = true
    assertEquals(testTheme.colors.red, button.currentTextColor)
    assertNull(button.stateListAnimator); assertEquals(0f, button.elevation)
  }
}
