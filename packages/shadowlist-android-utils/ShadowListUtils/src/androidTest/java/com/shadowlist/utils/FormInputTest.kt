package com.shadowlist.utils

import android.text.InputType
import android.view.inputmethod.EditorInfo
import org.junit.Assert.*
import org.junit.Test

class FormInputTest {
  @Test fun literal_keyboard_defaults_and_explicit_overrides() = onMain {
    val field = ShadowListUtilsFormInput(testContext, "12", "Amount", testTheme)
    assertTrue(field.input.inputType and InputType.TYPE_TEXT_FLAG_CAP_SENTENCES != 0)
    field.numeric = true
    assertEquals(InputType.TYPE_CLASS_NUMBER, field.input.inputType and InputType.TYPE_MASK_CLASS)
    assertTrue(field.input.inputType and InputType.TYPE_NUMBER_FLAG_DECIMAL != 0)
    assertEquals(0, field.input.inputType and InputType.TYPE_NUMBER_FLAG_SIGNED)
    field.keyboardType = ShadowListUtilsKeyboardType.NUMBER_PAD
    assertEquals(0, field.input.inputType and InputType.TYPE_NUMBER_FLAG_DECIMAL)
    field.keyboardType = ShadowListUtilsKeyboardType.EMAIL_ADDRESS
    field.autoCapitalize = ShadowListUtilsAutoCapitalize.CHARACTERS
    field.autoCorrect = true
    assertTrue(field.input.inputType and InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS != 0)
    assertTrue(field.input.inputType and InputType.TYPE_TEXT_FLAG_AUTO_CORRECT != 0)
    assertEquals("Amount", field.input.contentDescription)
  }
  @Test fun controlled_edit_and_submit_preserve_application_ownership() = onMain {
    val field = ShadowListUtilsFormInput(testContext, "before", "Name", testTheme)
    val changes = mutableListOf<String>()
    field.onChange = { changes.add(it) }
    field.value = "programmatic"
    assertTrue(changes.isEmpty())
    field.input.setText("edit")
    assertEquals(listOf("edit"), changes)
    assertEquals("programmatic", field.value)
    assertEquals("programmatic", field.input.text.toString())
    var submits = 0
    field.onSubmitEditing = { submits++ }
    field.input.onEditorAction(EditorInfo.IME_ACTION_DONE)
    assertEquals(1, submits)
  }
  @Test fun large_text_uses_native_sp_scaling_and_stacks_the_form() = onMain {
    val config = android.content.res.Configuration(testContext.resources.configuration).apply { fontScale = 1.5f }
    val context = testContext.createConfigurationContext(config)
    val field = ShadowListUtilsFormInput(context, "24", "Amount", testTheme)
    ShadowListUtilsFormRow(context, "Amount", listOf(field), testTheme)
    assertTrue(field.stacked)
    val expected = android.util.TypedValue.applyDimension(android.util.TypedValue.COMPLEX_UNIT_SP,
      testTheme.typography.body.fontSize, context.resources.displayMetrics)
    assertEquals(expected, field.input.textSize, 0.01f)
  }
  @Test fun row_retains_field_and_supports_explicit_stacking() = onMain {
    val field = ShadowListUtilsFormInput(testContext, "24", "Amount", testTheme)
    val row = ShadowListUtilsFormRow(testContext, "Amount", listOf(field), testTheme)
    assertFalse(field.stacked)
    row.stacked = true
    assertTrue(field.stacked)
    row.children = listOf(field)
    assertEquals("24", field.value)
    assertNotNull(field.parent)
  }
}
