package com.shadowlist.utils

import android.app.Activity
import android.content.Intent
import android.view.View
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class ShadowListUtilsAssistantTestActivity : Activity()

class AssistantMarkdownTest {
  @Test fun stable_blocks_keep_views_while_the_last_block_and_streaming_dot_grow() = onMain {
    val markdown = ShadowListUtilsAssistantMarkdown(testContext, "# Title\n\nA reply", testTheme)
    val first = (0 until markdown.childCount).map { markdown.getChildAt(it) }.filterIsInstance<ShadowListUtilsAssistantMarkdownBlock>().first()
    fun height(): Int {
      markdown.measure(View.MeasureSpec.makeMeasureSpec(250f.px, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
      return markdown.measuredHeight
    }
    val previous = height()
    markdown.text += " that keeps growing over several lines with a useful answer for the crew."
    assertSame(first, (0 until markdown.childCount).map { markdown.getChildAt(it) }.filterIsInstance<ShadowListUtilsAssistantMarkdownBlock>().first())
    assertTrue(height() > previous)
    val before = height()
    markdown.streaming = true
    assertTrue(height() > before)
  }

  @Test fun unsafe_links_are_blocked_and_custom_handlers_get_the_original_target() = onMain {
    val block = ShadowListUtilsCore.parseMarkdown("[Call](tel:+123)").first()
    val view = ShadowListUtilsAssistantMarkdownBlock(testContext, block, testTheme, ShadowListUtilsAssistantLabels())
    assertFalse(view.open("tel:+123"))
    var target: String? = null
    view.onOpenLink = { target = it }
    assertTrue(view.open("  tel:+123  "))
    assertEquals("  tel:+123  ", target)
  }

  @Test fun code_waits_for_a_closed_fence_and_copied_state_resets_after_delay_and_detach() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(Intent().setClassName(context.packageName,
      ShadowListUtilsAssistantTestActivity::class.java.name).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    lateinit var view: ShadowListUtilsAssistantMarkdownBlock
    lateinit var container: FrameLayout
    try {
      onMain {
        val open = ShadowListUtilsCore.parseMarkdown("```swift\nlet text = 1").first()
        val closed = ShadowListUtilsCore.parseMarkdown("```swift\nlet text = 1\n```").first()
        val pending = ShadowListUtilsAssistantMarkdownBlock(activity, open, testTheme, ShadowListUtilsAssistantLabels())
        assertEquals(View.GONE, pending.copyButton.visibility)
        view = ShadowListUtilsAssistantMarkdownBlock(activity, closed, testTheme, ShadowListUtilsAssistantLabels())
        container = FrameLayout(activity)
        container.addView(view)
        activity.setContentView(container)
        var copied: String? = null
        view.onCopyCode = { copied = it }
        view.copyButton.performClick()
        assertEquals("let text = 1", copied)
        assertEquals("Copied", view.copyButton.contentDescription)
      }
      Thread.sleep(1650)
      onMain {
        assertEquals("Copy code", view.copyButton.contentDescription)
        view.copyButton.performClick()
        assertEquals("Copied", view.copyButton.contentDescription)
        container.removeView(view)
        assertEquals("Copy code", view.copyButton.contentDescription)
      }
    } finally { onMain { activity.finish() } }
  }

  @Test fun the_parser_bridge_preserves_embedded_null_characters() {
    val text = "before\u0000after"
    val block = ShadowListUtilsCore.parseMarkdown(text).first()
    assertEquals(text, block.raw)
    assertEquals(text, block.runs.first().first().text)
  }
}
