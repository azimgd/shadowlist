package com.shadowlist.utils

import android.view.View
import org.junit.Assert.*
import org.junit.Test

class FieldTest {
  @Test fun text_field_mode_preserves_controlled_value_and_silence() = onMain {
    val field = ShadowListUtilsTextField(testContext, "first\nsecond", testTheme)
    assertEquals(android.view.inputmethod.EditorInfo.IME_ACTION_DONE, field.input.imeOptions and android.view.inputmethod.EditorInfo.IME_MASK_ACTION)
    val changes = mutableListOf<String>()
    field.onChange = { changes.add(it) }
    field.value = "updated"; field.multiline = true
    assertEquals("updated", field.input.text.toString())
    field.measure(View.MeasureSpec.makeMeasureSpec(200f.px, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    assertEquals(104f.px, field.measuredHeight)
    field.multiline = false
    field.placeholder = "Notes"
    assertEquals("Notes", field.input.contentDescription)
    field.accessibilityLabel = "Travel notes"
    field.placeholder = "New hint"
    assertEquals("Travel notes", field.input.contentDescription)
    field.keyboardType = ShadowListUtilsKeyboardType.NUMERIC
    assertTrue(field.input.inputType and android.text.InputType.TYPE_NUMBER_FLAG_SIGNED != 0)
    field.keyboardType = ShadowListUtilsKeyboardType.DECIMAL_PAD
    assertEquals(0, field.input.inputType and android.text.InputType.TYPE_NUMBER_FLAG_SIGNED)
    assertEquals("updated", field.input.text.toString())
    assertTrue(changes.isEmpty())
  }
  @Test fun search_labels_and_custom_icon_preserve_caller_semantics() = onMain {
    val field = ShadowListUtilsSearchField(testContext, "Lisbon", testTheme)
    assertEquals(android.view.inputmethod.EditorInfo.IME_ACTION_DONE, field.input.imeOptions and android.view.inputmethod.EditorInfo.IME_MASK_ACTION)
    val changes = mutableListOf<String>()
    field.onChange = { changes.add(it) }
    field.value = "Porto"; field.labels = ShadowListUtilsSearchFieldLabels("Find route")
    assertEquals("Porto", field.input.text.toString())
    assertEquals("Find route", field.input.contentDescription)
    assertTrue(changes.isEmpty())
    val icon = android.widget.Button(testContext).apply {
      layoutParams = android.view.ViewGroup.LayoutParams(32f.px, 24f.px)
      contentDescription = "Filter options"; importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
    }
    var presses = 0
    icon.setOnClickListener { presses++ }
    field.icon = icon
    assertSame(field, icon.parent)
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_YES, icon.importantForAccessibility)
    assertTrue(icon.isClickable)
    icon.performClick(); assertEquals(1, presses)
    assertEquals("Filter options", icon.contentDescription)
    field.measure(View.MeasureSpec.makeMeasureSpec(200f.px, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    field.layout(0, 0, field.measuredWidth, field.measuredHeight)
    assertEquals(32f.px, icon.measuredWidth); assertEquals(24f.px, icon.measuredHeight)
    assertEquals(48f.px, (field.input.layoutParams as android.widget.FrameLayout.LayoutParams).marginStart)
    field.icon = null
    field.measure(View.MeasureSpec.makeMeasureSpec(200f.px, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    assertNull(icon.parent)
    assertEquals(32f.px, (field.input.layoutParams as android.widget.FrameLayout.LayoutParams).marginStart)
  }
}
