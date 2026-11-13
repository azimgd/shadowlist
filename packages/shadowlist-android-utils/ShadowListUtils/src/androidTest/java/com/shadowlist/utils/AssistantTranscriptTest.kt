package com.shadowlist.utils

import android.os.Looper
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AssistantTranscriptTest {
  private val theme = testTheme
  private val saved = ShadowListUtilsAssistantTurn("Saved reply", status = ShadowListUtilsAssistantTurnStatus.Done())
  private fun reply(turn: ShadowListUtilsAssistantTurn = saved) = ShadowListUtilsAssistantReply("reply", listOf(turn))
  private fun messages(turn: ShadowListUtilsAssistantTurn = saved): List<ShadowListUtilsAssistantMessage> = listOf(
    ShadowListUtilsAssistantMessage.User(ShadowListUtilsAssistantPrompt("user", "Prompt")),
    ShadowListUtilsAssistantMessage.Assistant(reply(turn)), ShadowListUtilsAssistantMessage.endMarker)
  private fun views(view: View): Sequence<View> = sequence {
    yield(view)
    if (view is ViewGroup) for (index in 0 until view.childCount) yieldAll(views(view.getChildAt(index)))
  }
  private fun action(view: View, label: String): ShadowListUtilsAssistantActionButton = views(view).filterIsInstance<ShadowListUtilsAssistantActionButton>().first { it.label == label }
  private fun button(view: View, label: String): Button = views(view).filterIsInstance<Button>().first { it.contentDescription?.toString() == label }

  @Test fun live_turn_overrides_saved_variants_and_invalid_variants_are_empty_done() = onMain {
    val current = reply()
    val live = ShadowListUtilsAssistantTurn("Live reply")
    assertSame(live, ShadowListUtilsAssistantReplyMessage.resolvedTurn(current, live))
    assertEquals(saved, ShadowListUtilsAssistantReplyMessage.resolvedTurn(current, null))
    val missing = ShadowListUtilsAssistantReplyMessage.resolvedTurn(current.copy(variantIndex = 8), null)
    assertEquals("", missing.content)
    assertTrue(missing.status is ShadowListUtilsAssistantTurnStatus.Done)
    val list = ShadowListUtilsAssistantList(testContext, theme)
    list.items = messages()
    list.turns = mapOf("reply" to live)
    val row = list.rows[1] as ShadowListUtilsAssistantRow
    assertEquals("Live reply", row.turn?.content)
    assertEquals(listOf(saved), (list.items[1] as ShadowListUtilsAssistantMessage.Assistant).reply.variants)
    assertTrue(list.inverted)
    assertTrue(list.followAppends)
    assertTrue(ShadowListUtilsAssistantMessage.endMarker.key in list.nonAnchorKeys)
    assertEquals(1f.px, list.rows.last().layout(390f.px).heightPx)
  }

  @Test fun thinking_and_tool_disclosures_rebuild_row_height_and_preserve_keyed_state() = onMain {
    val turn = ShadowListUtilsAssistantTurn("Reply", "A long thought. ".repeat(80), 2000.0,
      listOf(ShadowListUtilsAssistantToolCall("tool", "Inspect", "Long input.\n".repeat(30), 0, ShadowListUtilsAssistantToolStatus.Done("Long output.\n".repeat(30)))),
      ShadowListUtilsAssistantTurnStatus.Done())
    val list = ShadowListUtilsAssistantList(testContext, theme)
    list.items = messages(turn)
    host(list)
    var row = list.rows[1] as ShadowListUtilsAssistantRow
    val before = row.layout(390f.px).heightPx
    var view = list.rowView(1)!!
    button(view, "Thought for 2s").performClick()
    host(list)
    row = list.rows[1] as ShadowListUtilsAssistantRow
    assertTrue(row.thinkingExpanded)
    assertTrue(row.layout(390f.px).heightPx > before)
    list.items = messages(turn)
    host(list)
    row = list.rows[1] as ShadowListUtilsAssistantRow
    assertTrue(row.thinkingExpanded)
    view = list.rowView(1)!!
    button(view, "Inspect, Done").performClick()
    host(list)
    assertTrue("tool" in (list.rows[1] as ShadowListUtilsAssistantRow).expandedToolKeys)
    list.items = emptyList()
    list.items = messages(turn)
    assertFalse((list.rows[1] as ShadowListUtilsAssistantRow).thinkingExpanded)
    assertTrue((list.rows[1] as ShadowListUtilsAssistantRow).expandedToolKeys.isEmpty())
  }

  @Test fun user_actions_and_busy_editing_use_the_transcript_state() = onMain {
    val list = ShadowListUtilsAssistantList(testContext, theme)
    list.items = messages()
    var edits = 0
    list.onEdit = { edits++ }
    host(list)
    val view = list.rowView(0)!!
    val bubble = views(view).first { it.contentDescription?.toString() == "Prompt" && it.isLongClickable }
    bubble.performLongClick()
    host(list)
    assertTrue((list.rows[0] as ShadowListUtilsAssistantRow).actionsVisible)
    list.streaming = true
    host(list)
    assertFalse(action(list.rowView(0)!!, "Edit").isEnabled)
    action(list.rowView(0)!!, "Edit").performClick()
    assertEquals(0, edits)
  }

  @Test fun streaming_keeps_action_space_but_removes_touch_and_accessibility_and_reuses_markdown() = onMain {
    val view = ShadowListUtilsAssistantReplyMessage(testContext, reply(), ShadowListUtilsAssistantTurn("First paragraph."), theme)
    var copied = ""
    view.onCopy = { copied = it }
    host(view)
    val copy = action(view, "Copy")
    val markdown = views(view).filterIsInstance<ShadowListUtilsAssistantMarkdown>().first()
    assertEquals(View.INVISIBLE, copy.visibility)
    assertFalse(copy.isEnabled)
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS, copy.importantForAccessibility)
    copy.performClick()
    assertEquals("", copied)
    view.turn = ShadowListUtilsAssistantTurn("First paragraph.\n\nSecond paragraph.")
    host(view)
    assertSame(markdown, views(view).filterIsInstance<ShadowListUtilsAssistantMarkdown>().first())
    view.turn = saved
    host(view)
    assertSame(copy, action(view, "Copy"))
    assertEquals(View.VISIBLE, copy.visibility)
    copy.performClick()
    assertEquals("Saved reply", copied)
  }

  @Test fun variants_feedback_and_retry_remain_application_owned() = onMain {
    val model = reply().copy(variants = listOf(saved, saved.copy(content = "Second")), feedback = ShadowListUtilsAssistantFeedback.GOOD)
    val view = ShadowListUtilsAssistantReplyMessage(testContext, model, theme = theme)
    var variant = -1
    var feedback: ShadowListUtilsAssistantFeedback? = ShadowListUtilsAssistantFeedback.BAD
    var regenerated = 0
    view.onSelectVariant = { _, index -> variant = index }
    view.onFeedback = { _, value -> feedback = value }
    view.onRegenerate = { regenerated++ }
    host(view)
    action(view, "Next version").performClick()
    action(view, "Good response").performClick()
    assertEquals(1, variant)
    assertNull(feedback)
    assertEquals(0, view.message.variantIndex)
    assertEquals(ShadowListUtilsAssistantFeedback.GOOD, view.message.feedback)
    view.busy = true
    host(view)
    action(view, "Regenerate").performClick()
    assertEquals(0, regenerated)
    view.turn = ShadowListUtilsAssistantTurn(status = ShadowListUtilsAssistantTurnStatus.Failed("Failure"))
    var retries = 0
    view.onRetry = { retries++ }
    host(view)
    assertFalse(button(view, "Retry").isEnabled)
    view.busy = false
    host(view)
    button(view, "Retry").performClick()
    assertEquals(1, retries)
  }

  @Test fun only_the_latest_completed_reply_has_follow_ups_and_source_callbacks_keep_original_urls() = onMain {
    val url = "  https://example.com/source  "
    val turn = saved.copy(status = ShadowListUtilsAssistantTurnStatus.Done(listOf(ShadowListUtilsAssistantSource("s", "Source", url)), listOf("Continue?")))
    val view = ShadowListUtilsAssistantReplyMessage(testContext, reply(turn), theme = theme)
    var opened = ""
    var followUp = ""
    view.onOpenLink = { opened = it }
    view.onFollowUp = { followUp = it }
    host(view)
    assertFalse(views(view).any { it.contentDescription?.toString() == "Continue?" })
    view.isLatest = true
    host(view)
    button(view, "Source 1: Source").performClick()
    button(view, "Continue?").performClick()
    assertEquals(url, opened)
    assertEquals("Continue?", followUp)
    view.turn = ShadowListUtilsAssistantTurn("Live")
    host(view)
    assertFalse(views(view).any { it.contentDescription?.toString() == "Continue?" || it.contentDescription?.toString() == "Source 1: Source" })
  }

  @Test fun background_layout_uses_frozen_collections_and_ui_resolved_formatters() {
    lateinit var row: ShadowListUtilsAssistantRow
    val calls = ArrayList<ShadowListUtilsAssistantToolCall>()
    val sources = ArrayList<ShadowListUtilsAssistantSource>()
    val followUps = arrayListOf("Original follow up")
    val variants = arrayListOf(saved.copy(thinking = "Thought", thinkingMs = 2000.0, toolCalls = calls,
      status = ShadowListUtilsAssistantTurnStatus.Done(sources, followUps)))
    onMain {
      fun checkOwner(): String { check(Looper.myLooper() == Looper.getMainLooper()); return "Localized" }
      val list = ShadowListUtilsAssistantList(testContext, theme)
      list.labels = ShadowListUtilsAssistantLabels(thoughtFor = { checkOwner() }, toolStatus = { checkOwner() },
        source = { _, _ -> checkOwner() }, versionPosition = { _, _ -> checkOwner() })
      calls.add(ShadowListUtilsAssistantToolCall("tool", "Tool", "Input", status = ShadowListUtilsAssistantToolStatus.Done("Output")))
      sources.add(ShadowListUtilsAssistantSource("s", "Source", "https://example.com"))
      list.items = listOf(ShadowListUtilsAssistantMessage.Assistant(reply().copy(variants = variants)), ShadowListUtilsAssistantMessage.endMarker)
      row = list.rows[0] as ShadowListUtilsAssistantRow
      variants.clear(); calls.clear(); sources.clear(); followUps.clear()
    }
    val worker = Executors.newSingleThreadExecutor()
    try {
      val layout = worker.submit<ShadowListUtilsRowLayout> { row.layout(390f.px) }.get(3, TimeUnit.SECONDS) as ShadowListUtilsAssistantRowLayout
      assertTrue(layout.heightPx > 0)
      val body = (layout.content as ShadowListUtilsAssistantRowLayout.Content.Assistant).layout
      assertTrue(body.sections.any { it is ShadowListUtilsAssistantReplyMessage.Section.Source })
      assertTrue(body.sections.any { it is ShadowListUtilsAssistantReplyMessage.Section.FollowUp })
    } finally { worker.shutdownNow() }
  }

  @Test fun hidden_thinking_and_variant_ui_never_invokes_its_formatters() = onMain {
    val list = ShadowListUtilsAssistantList(testContext, theme)
    list.labels = ShadowListUtilsAssistantLabels(thoughtFor = { error("No thinking to format") },
      versionPosition = { _, _ -> error("No pager to format") })
    list.items = listOf(ShadowListUtilsAssistantMessage.Assistant(ShadowListUtilsAssistantReply("empty")), ShadowListUtilsAssistantMessage.endMarker)
    assertTrue(list.rows[0].layout(390f.px).heightPx > 0)
    host(list)
  }

  @Test fun composer_controls_drafts_send_stop_and_attachments_without_disabling_typing() = onMain {
    val composer = ShadowListUtilsAssistantComposer(testContext, theme)
    val sent = ArrayList<Pair<String, List<ShadowListUtilsAssistantAttachment>>>()
    var stopped = 0
    composer.onSend = { text, files -> sent.add(text to files) }
    composer.onStop = { stopped++ }
    composer.setDraft("\uFEFF  Hello  \uFEFF", focus = false)
    composer.sendOrStop()
    assertEquals("Hello", sent.single().first)
    assertEquals("", composer.getDraft())
    composer.streaming = true
    composer.setDraft("Keep typing", focus = false)
    assertTrue(composer.input.input.isEnabled)
    assertEquals("Stop generating", composer.primaryAction.contentDescription)
    composer.sendOrStop()
    assertEquals(1, stopped)
    assertEquals("Keep typing", composer.getDraft())
    composer.sendWhileStreaming = true
    assertEquals("Send", composer.primaryAction.contentDescription)
    composer.sendOrStop()
    assertEquals("Keep typing", sent.last().first)
    assertEquals("Stop generating", composer.primaryAction.contentDescription)
    composer.attachments = listOf(ShadowListUtilsAssistantAttachment("file", ShadowListUtilsAssistantAttachment.Kind.FILE, "notes.md"))
    composer.sendOrStop()
    assertEquals("", sent.last().first)
    assertEquals("file", sent.last().second.single().key)
    assertEquals(1, composer.attachments.size)
  }

  @Test fun controlled_drafts_and_optional_composer_actions_request_application_changes() = onMain {
    val composer = ShadowListUtilsAssistantComposer(testContext, theme)
    val changes = ArrayList<String>()
    composer.value = "Owned"
    composer.onChangeText = { changes.add(it) }
    composer.setDraft("Proposed", focus = false)
    assertEquals("Owned", composer.getDraft())
    assertTrue("Proposed" in changes)
    composer.clearDraft()
    assertTrue("" in changes)
    assertEquals("Owned", composer.getDraft())
    composer.value = null
    composer.setDraft("x".repeat(4001), focus = false)
    assertEquals(4000, composer.getDraft().length)
    var requested: Boolean? = null
    composer.onThinkingChange = { requested = it }
    host(composer)
    button(composer, "Extended thinking").performClick()
    assertEquals(true, requested)
    assertFalse(composer.thinking)
  }
}
