package com.shadowlist.utils

import android.view.View
import android.view.ViewGroup
import android.widget.Button
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AssistantComponentsTest {
  private val theme = testTheme
  private fun measure(view: View, width: Int): Int {
    view.measure(View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    view.layout(0, 0, width, view.measuredHeight)
    return view.measuredHeight
  }

  @Test fun hidden_scroll_button_retains_its_slot_and_cannot_act_or_take_accessibility_focus() = onMain {
    val button = ShadowListUtilsAssistantScrollButton(testContext, theme)
    var presses = 0
    button.onPress = { presses++ }
    button.performClick()
    assertEquals(0, presses)
    assertFalse(button.isClickable)
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS, button.importantForAccessibility)
    button.visible = true
    button.performClick()
    assertEquals(1, presses)
    val height = measure(button, 36f.px)
    button.visible = false
    assertEquals(height, measure(button, 36f.px))
    button.onPress = null
    button.visible = true
    assertFalse(button.isEnabled)
  }

  @Test fun thinking_disclosure_preserves_UTF16_tail_and_application_text() = onMain {
    val text = "Prefix " + "Reasoning 👋🏽 text. ".repeat(50)
    val view = ShadowListUtilsAssistantThinking(testContext, text, true, theme)
    val before = measure(view, 300f.px)
    val layout = ShadowListUtilsAssistantThinking.layout(text, "Thinking", false, 300f.px, theme)
    assertEquals(text.takeLast(160), layout.text)
    view.toggle()
    assertTrue(measure(view, 300f.px) > before)
    assertEquals(text, view.thinking)
    assertEquals("Thought for 1s", ShadowListUtilsAssistantThinking.title(1499.0, false, view.labels))
    assertEquals("Thought for 2s", ShadowListUtilsAssistantThinking.title(1500.0, false, view.labels))
    assertEquals("Thinking", ShadowListUtilsAssistantThinking.title(Double.NaN, true, view.labels))
  }

  @Test fun tool_disclosure_uses_the_same_immutable_geometry_as_transcript_without_mutating_call() = onMain {
    val call = ShadowListUtilsAssistantToolCall("search", "Search", "Input\n".repeat(20), status = ShadowListUtilsAssistantToolStatus.Done("Output\n".repeat(20)))
    val view = ShadowListUtilsAssistantToolCallCard(testContext, call, theme)
    val before = measure(view, 300f.px)
    view.toggle()
    val after = measure(view, 300f.px)
    assertTrue(after > before)
    assertEquals(call, view.call)
    val reply = ShadowListUtilsAssistantReply("reply", listOf(ShadowListUtilsAssistantTurn(toolCalls = listOf(call), status = ShadowListUtilsAssistantTurnStatus.Done())))
    val layout = ShadowListUtilsAssistantReplyMessage.layout(reply, null, 332f.px, false, false, setOf(call.key), false, false, theme, view.labels,
      ShadowListUtilsAssistantReplyMessage.textSnapshot(reply, null, view.labels))
    val section = layout.sections.filterIsInstance<ShadowListUtilsAssistantReplyMessage.Section.Tool>().single()
    assertEquals(after, section.frame.height())
    view.call = ShadowListUtilsAssistantToolCall("other", "Run", "", status = ShadowListUtilsAssistantToolStatus.Stopped)
    assertFalse(view.expanded)
    view.grouped = true
    view.separated = true
  }

  @Test fun empty_suggestions_request_selection_without_mutating_model_and_wrap_at_narrow_width() = onMain {
    val view = ShadowListUtilsAssistantEmpty(testContext, theme)
    val item = ShadowListUtilsAssistantSuggestion("Plan a trip", "Three days in Lisbon")
    view.suggestions = listOf(item, item)
    var selected: ShadowListUtilsAssistantSuggestion? = null
    view.onSelectSuggestion = { selected = it }
    (0 until view.childCount).map(view::getChildAt).filterIsInstance<Button>().first().performClick()
    assertEquals(item, selected)
    assertEquals(listOf(item, item), view.suggestions)
    val narrow = measure(view, 210f.px)
    assertTrue(narrow > measure(view, 390f.px))
    view.labels = view.labels.copy(emptyTitle = "Where next?", emptySubtitle = "Choose a prompt")
    measure(view, 390f.px)
  }

  @Test fun typing_indicator_exposes_one_localized_progress_element_and_detaches_cleanly() = onMain {
    val view = ShadowListUtilsAssistantTypingIndicator(testContext, theme)
    view.labels = view.labels.copy(typing = "Preparing answer")
    assertEquals("Preparing answer", view.contentDescription)
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_YES, view.importantForAccessibility)
    measure(view, 100f.px)
    assertEquals(3, view.childCount)
  }

  @Test fun poll_footer_localizes_total_and_list_preserves_custom_or_cleared_footer() = onMain {
    val view = ShadowListUtilsPollFooter(testContext, 3.0, theme)
    val labels = ShadowListUtilsPollLabels(totalVotes = { "Votes: ${it.toInt()}" })
    view.labels = labels
    view.totalVotes = 7.0
    measure(view, 300f.px)
    val inner = (0 until view.childCount).map(view::getChildAt).filterIsInstance<ShadowListUtilsListFooter>().single()
    assertEquals("Votes: 7", inner.text)
    val list = ShadowListUtilsPollList(testContext, theme)
    assertTrue(list.footerView is ShadowListUtilsPollFooter)
    val custom = View(testContext)
    list.footerView = custom
    list.poll = ShadowListUtilsPollData("Where?", emptyList())
    assertSame(custom, list.footerView)
    list.footerView = null
    list.labels = labels
    assertNull(list.footerView)
  }
  @Test fun selectable_native_text_paint_matches_prepared_lines_across_widths_and_unicode() = onMain {
    for (text in listOf("A selectable thought.\nNext line 👋🏽 https://example.com", "مرحبا بالعالم. ".repeat(8), "Text ending in a line break\n")) {
      for (width in listOf(100f.px, 280f.px)) {
        val layout = ShadowListUtilsAssistantDisclosureTextLayout(text, width, theme.typography.footnote, true)
        val view = ShadowListUtilsAssistantDisclosureTextView(testContext)
        val container = android.widget.FrameLayout(testContext)
        container.addView(view, android.widget.FrameLayout.LayoutParams(width, ViewGroup.LayoutParams.WRAP_CONTENT))
        layout.apply(view, theme.colors.secondaryLabel)
        measure(view, width)
        assertEquals(layout.height, view.layout.height)
        assertEquals(layout.layout.lineCount, view.layout.lineCount)
        assertTrue(view.isTextSelectable)
        assertFalse(view.linksClickable)
        assertEquals(0, view.paddingLeft)
        assertEquals(0, view.paddingTop)
        assertEquals(text, view.text.toString())
        val selectable = view.text as android.text.Spannable
        android.text.Selection.setSelection(selectable, 0, minOf(3, text.length))
        assertEquals(minOf(3, text.length), view.selectionEnd)
      }
    }
  }

  @Test fun native_disclosure_selection_copies_original_text_in_an_attached_window() {
    val instrumentation = androidx.test.platform.app.InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(android.content.Intent().setClassName(context.packageName,
      ShadowListUtilsAssistantTestActivity::class.java.name).addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK))
    val focused = java.util.concurrent.CountDownLatch(1)
    val text = "Copy 👋🏽 https://example.com\n"
    lateinit var view: ShadowListUtilsAssistantDisclosureTextView
    try {
      onMain {
        val layout = ShadowListUtilsAssistantDisclosureTextLayout(text, 280f.px, theme.typography.footnote, true)
        view = ShadowListUtilsAssistantDisclosureTextView(activity)
        val container = android.widget.FrameLayout(activity)
        container.addView(view, android.widget.FrameLayout.LayoutParams(280f.px, ViewGroup.LayoutParams.WRAP_CONTENT))
        activity.setContentView(container)
        layout.apply(view, theme.colors.secondaryLabel)
        measure(view, 280f.px)
        activity.window.decorView.viewTreeObserver.addOnWindowFocusChangeListener { hasFocus -> if (hasFocus) focused.countDown() }
        if (activity.window.decorView.hasWindowFocus()) focused.countDown()
      }
      assertTrue("The real test activity must own foreground window focus before accessing its clipboard", focused.await(5, java.util.concurrent.TimeUnit.SECONDS))
      onMain {
        assertTrue(view.requestFocus())
        android.text.Selection.setSelection(view.text as android.text.Spannable, 0, text.length)
        assertTrue(view.onTextContextMenuItem(android.R.id.copy))
        val clipboard = activity.getSystemService(android.content.Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
        assertEquals(text, clipboard.primaryClip?.getItemAt(0)?.text?.toString())
        assertEquals(text, view.text.toString())
      }
    } finally { onMain { activity.finish() } }
  }

}
