package com.shadowlist.utils

import android.content.Intent
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Test

class TextInputTest {
  @Test fun programmatic_values_are_silent_and_unchanged_renders_preserve_selection() = onMain {
    val input = ShadowListUtilsTextInputEditText(testContext)
    val editing = ShadowListUtilsTextInput(input)
    val changes = mutableListOf<String>()
    editing.onChangeText = { changes.add(it) }
    editing.value = "flight"; input.setSelection(2)
    editing.value = "flight"
    assertEquals(2, input.selectionStart)
    assertTrue(changes.isEmpty())
    editing.value = "updated"
    assertEquals("updated", input.text.toString())
    assertEquals(2, input.selectionStart)
    assertTrue(changes.isEmpty())
  }

  @Test fun rejected_and_accepted_controlled_edits_request_exactly_one_change() = onMain {
    val input = ShadowListUtilsTextInputEditText(testContext)
    val editing = ShadowListUtilsTextInput(input)
    val changes = mutableListOf<String>()
    editing.value = "Saved"
    editing.onChangeText = { changes.add(it) }
    input.setText("Candidate")
    assertEquals(listOf("Candidate"), changes)
    assertEquals("Saved", input.text.toString())
    editing.onChangeText = { changes.add(it); editing.value = it }
    input.setText("Accepted")
    assertEquals(listOf("Candidate", "Accepted"), changes)
    assertEquals("Accepted", input.text.toString())
  }

  @Test fun utf16_length_limit_does_not_split_a_surrogate_pair() = onMain {
    val input = ShadowListUtilsTextInputEditText(testContext)
    val editing = ShadowListUtilsTextInput(input)
    editing.maxLength = 3
    input.setText("AB👩‍👩‍👧‍👧C")
    assertEquals("AB", input.text.toString())
  }

  @Test fun real_input_connection_preserves_composition_until_finish_and_submits_once() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(Intent().setClassName(context.packageName,
      ShadowListUtilsAssistantTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    val focused = CountDownLatch(1)
    lateinit var field: ShadowListUtilsFormInput
    try {
      onMain {
        field = ShadowListUtilsFormInput(activity, "", "Name", testTheme)
        val container = FrameLayout(activity)
        container.addView(field, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 80f.px))
        activity.setContentView(container)
        activity.window.decorView.viewTreeObserver.addOnWindowFocusChangeListener { if (it) focused.countDown() }
        if (activity.window.decorView.hasWindowFocus()) focused.countDown()
      }
      assertTrue("The attached editor must own the real foreground window", focused.await(5, TimeUnit.SECONDS))
      onMain {
        assertTrue(field.input.requestFocus())
        val info = EditorInfo()
        val connection = requireNotNull(field.input.onCreateInputConnection(info))
        val changes = mutableListOf<String>()
        field.onChange = { changes.add(it) }
        assertTrue(connection.setComposingText("に", 1))
        assertEquals("に", field.input.text.toString())
        field.value = "Owner"
        assertEquals("に", field.input.text.toString())
        assertTrue(connection.finishComposingText())
        assertEquals("Owner", field.input.text.toString())
        assertEquals(listOf("に"), changes)
        var submits = 0
        field.input.onFocusChangeListener = android.view.View.OnFocusChangeListener { _, hasFocus ->
          if (!hasFocus) (field.parent as? ViewGroup)?.removeView(field)
        }
        field.onSubmitEditing = { submits++ }
        assertTrue(connection.performEditorAction(EditorInfo.IME_ACTION_DONE))
        assertEquals(1, submits)
        assertFalse(field.input.hasFocus())
        assertNull(field.parent)
      }
    } finally { onMain { activity.finish() } }
  }
}
