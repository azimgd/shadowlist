package com.shadowlist.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ChatInputTest {
  @Test fun an_uncontrolled_draft_trims_sends_and_clears() = onMain {
    val composer = ShadowListUtilsChatInput(testContext, "  Hello crew ")
    val sent = ArrayList<String>()
    composer.onSend = { sent.add(it) }
    composer.sendText()
    assertEquals(listOf("Hello crew"), sent)
    assertEquals("", composer.input.text.toString())
    assertFalse(composer.send.isEnabled)
    composer.sendText()
    assertEquals(1, sent.size)
  }

  @Test fun a_controlled_value_only_changes_when_the_application_answers() = onMain {
    val composer = ShadowListUtilsChatInput(testContext, "draft")
    composer.value = "Current"
    val changes = ArrayList<String>()
    val sent = ArrayList<String>()
    composer.onChangeText = { changes.add(it) }
    composer.onSend = { sent.add(it) }
    composer.input.setText("Requested")
    assertEquals(listOf("Requested"), changes)
    assertEquals("Current", composer.input.text.toString())
    composer.sendText()
    assertEquals(listOf("Current"), sent)
    assertEquals("Current", composer.input.text.toString())
    composer.onChangeText = { composer.value = it }
    composer.input.setText("Accepted")
    assertEquals("Accepted", composer.value)
  }

  @Test fun disabled_whitespace_limits_and_labels_follow_the_contract() = onMain {
    val composer = ShadowListUtilsChatInput(testContext, " ")
    assertFalse(composer.send.isEnabled)
    composer.value = "Hello"
    composer.disabled = true
    var sent = false
    composer.onSend = { sent = true }
    composer.sendText()
    assertFalse(sent)
    assertFalse(composer.input.isEnabled)
    composer.disabled = false
    composer.value = null
    composer.maxLength = 1
    composer.input.setText("😀")
    assertEquals("", composer.input.text.toString())
    composer.input.setText("A")
    assertTrue(composer.send.isEnabled)
    composer.labels = composer.labels.copy(send = "Envoyer")
    assertEquals("Envoyer", composer.send.contentDescription)
  }
}
